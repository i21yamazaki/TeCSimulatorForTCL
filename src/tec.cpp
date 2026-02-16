#include <array>
#include <cassert>
#include <cstdint>
#include <deque>
#include <format>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

/// @brief エラーの種類
enum class ErrorType : uint8_t {
  /// @brief プログラムに問題がある。
  Program,
  /// @brief 判定用の入力に問題がある。
  Input,
  /// @brief バイナリに問題がある。
  Binary,
  /// @brief 名前表に問題がある。
  NameTable,
  /// @brief ジャッジシステムのバグ
  Bug
};

/// @brief エラー発生フラグ
static bool HasErrorOccurred = false;

/// @brief エラーが発生していれば出力して終了する。
static inline void CheckError() {
  if (HasErrorOccurred) {
    std::exit(1);
  }
}

/// @brief エラーメッセージを出力する。
/// @param msg エラーメッセージ
/// @param type エラーの種類
static void PrintError(const std::string &msg, const ErrorType type) {
  switch (type) {
  case ErrorType::Binary:
    std::cerr << "機械語: " << msg;
    break;
  case ErrorType::NameTable:
    std::cerr << "名前表: " << msg;
    break;
  case ErrorType::Input:
    std::cerr << "入力: " << msg;
    break;
  case ErrorType::Program:
    std::cerr << "エラー: " << msg;
    break;
  case ErrorType::Bug:
    std::cerr << "バグ: " << msg;
    break;
  }
  std::cerr << '\n';
  HasErrorOccurred = true;
}

/// @brief エラーメッセージを出力して終了する。
/// @param msg エラーメッセージ
/// @param type エラーの種類
[[noreturn]] static void Error(const std::string &msg, const ErrorType type) {
  PrintError(msg, type);
  std::exit(1);
}

// バグ発生時用マクロ
#define BUG(msg)                                                               \
  Error(std::format("{}:{}: {}", __FILE__, __LINE__, msg), ErrorType::Bug)

/// @brief 名前表
using NameTable = std::unordered_map<std::string, uint8_t>;

/// @brief レジスタ
enum class Reg : uint8_t { G0, G1, G2, SP, PC };

/// @brief 文字列をレジスタに変換する。
[[nodiscard]] static inline std::optional<Reg> StrToReg(const std::string &s) {
  std::optional<Reg> reg = std::nullopt;
  if (s == "G0") {
    reg = Reg::G0;
  } else if (s == "G1") {
    reg = Reg::G1;
  } else if (s == "G2") {
    reg = Reg::G2;
  } else if (s == "SP") {
    reg = Reg::SP;
  } else if (s == "PC") {
    reg = Reg::PC;
  }
  return reg;
}

/// @brief フラグ
enum class Flg : uint8_t { C, S, Z };

/// @brief 文字列をフラグに変換する。
[[nodiscard]] static inline std::optional<Flg> StrToFlg(const std::string &s) {
  std::optional<Flg> flg = std::nullopt;
  if (s == "C") {
    flg = Flg::C;
  } else if (s == "S") {
    flg = Flg::S;
  } else if (s == "Z") {
    flg = Flg::Z;
  }
  return flg;
}

/// @brief 判定用の簡易TeCシミュレータ
class TeC {
public:
  TeC() noexcept
      : m_g0(0x00), m_g1(0x00), m_g2(0x00), m_sp(0x00), m_pc(0x00), m_c(false),
        m_s(false), m_z(false), m_intEna(false), m_run(false), m_err(false),
        m_mm({
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0x00
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0x08
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0x10
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0x18
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0x20
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0x28
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0x30
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0x38
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0x40
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0x48
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0x50
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0x58
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0x60
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0x68
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0x70
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0x78
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0x80
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0x88
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0x90
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0x98
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0xA0
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0xA8
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0xB0
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0xB8
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0xC0
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0xC8
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0xD0
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0xD8
            0x1F, 0xDC, 0xB0, 0xF6, 0xD0, 0xD6, 0xB0, 0xF6, // 0xE0
            0xD0, 0xDA, 0xA4, 0xFF, 0xB0, 0xF6, 0x21, 0x00, // 0xE8
            0x37, 0x01, 0x4B, 0x01, 0xA0, 0xEA, 0xC0, 0x03, // 0xF0
            0x63, 0x40, 0xA4, 0xF6, 0xC0, 0x02, 0xEC, 0xFF  // 0xF8
        }),
        m_dataSW(0x00), m_rxReg(0x00), m_txReg(0x00), m_tmrCnt(0x00),
        m_tmrPeriod(74), m_parallelIn(0x00), m_parallelOut(0x00),
        m_extParallelOut(0x0), m_adcChs({0x00, 0x00, 0x00, 0x00}), m_buz(false),
        m_spk(false), m_txEmpty(true), m_rxFull(false), m_txIntEna(false),
        m_rxIntEna(false), m_tmrEna(false), m_tmrIntEna(false),
        m_cslIntEna(false), m_extParallelOutEna(false), m_tmrElapsed(false),
        m_int0(false), m_int3(false), m_tmrClkCnt(0) {}

  /// @brief 動作周波数 2.4576 MHz
  static constexpr uint64_t StatesPerSec = 2'457'600;

  /// @brief シリアル入出力の速度 9600 bit/s
  static constexpr uint64_t SIOBitPerSec = 9'600;

  /// @brief 実行を開始する。
  void run() noexcept { m_run = true; }

  /// @brief 実行を停止する。
  void stop() noexcept { m_run = false; }

  /// @brief 初期化する。
  void reset() noexcept {
    m_run = false;
    m_err = false;
    m_g0 = 0;
    m_g1 = 0;
    m_g2 = 0;
    m_sp = 0;
    m_pc = 0;
    m_txEmpty = true;
    m_rxFull = false;
    m_txIntEna = false;
    m_rxIntEna = false;
  }

  /// @brief レジスタの値を設定する。
  /// @param reg レジスタ
  /// @param val 値
  void setReg(const Reg reg, const uint8_t val) noexcept {
    switch (reg) {
    case Reg::G0:
      m_g0 = val;
      break;
    case Reg::G1:
      m_g1 = val;
      break;
    case Reg::G2:
      m_g2 = val;
      break;
    case Reg::SP:
      m_sp = val;
      break;
    case Reg::PC:
      m_pc = val;
      break;
    default:
      BUG("TeC::setReg(Reg, uint8_t) noexcept");
      break;
    }
  }

  /// @brief フラグの値を設定する。
  /// @param flg フラグ
  /// @param val 値
  void setFlg(const Flg flg, const bool val) noexcept {
    switch (flg) {
    case Flg::C:
      m_c = val;
      break;
    case Flg::S:
      m_s = val;
      break;
    case Flg::Z:
      m_z = val;
      break;
    default:
      BUG("TeC::setFlg(Flg, bool) noexcept");
      break;
    }
  }

  /// @brief 主記憶の値を設定する。
  /// @param addr アドレス
  /// @param val 値
  void setMM(const uint8_t addr, const uint8_t val) noexcept {
    writeMem(addr, val);
  }

  /// @brief データスイッチの値を設定する。
  /// @param val 値
  void setDataSW(const uint8_t val) noexcept { m_dataSW = val; }

  /// @brief レジスタの値を取得する。
  /// @param reg レジスタ
  /// @return 値
  uint8_t getReg(const Reg reg) const noexcept {
    uint8_t val = 0x00;
    switch (reg) {
    case Reg::G0:
      val = m_g0;
      break;
    case Reg::G1:
      val = m_g1;
      break;
    case Reg::G2:
      val = m_g2;
      break;
    case Reg::SP:
      val = m_sp;
      break;
    case Reg::PC:
      val = m_pc;
      break;
    default:
      BUG("TeC::getReg(Reg) const noexcept");
      break;
    }
    return val;
  }

  /// @brief ブザーの値を取得する。
  /// @return ブザーの値
  bool getBuz() const noexcept { return m_buz; }

  /// @brief スピーカの値を取得する。
  /// @return スピーカの値
  bool getSpk() const noexcept { return m_spk; }

  /// @brief フラグの値を取得する。
  /// @param flg フラグ
  /// @return 値
  bool getFlg(const Flg flg) const noexcept {
    bool val = false;
    switch (flg) {
    case Flg::C:
      val = m_c;
      break;
    case Flg::S:
      val = m_s;
      break;
    case Flg::Z:
      val = m_z;
      break;
    default:
      BUG("TeC::getFlg(Flg) const noexcept");
      break;
    }
    return val;
  }

  /// @brief 主記憶の値を取得する。
  /// @param addr アドレス
  /// @return 値
  uint8_t getMM(const uint8_t addr) const noexcept { return m_mm[addr]; }

  /// @brief 実行フラグの値を取得する。
  /// @return 実行フラグの値
  bool isRunning() const noexcept { return m_run; }

  /// @brief エラーフラグの値を取得する。
  /// @return エラーフラグの値
  bool isError() const noexcept { return m_err; }

  /// @brief SIOで1バイト送信するのに必要なステート数
  static constexpr uint64_t SerialUnitStates =
      StatesPerSec / (SIOBitPerSec * 8);

  /// @brief 指定したステート数の命令を実行する。
  /// @param maxStates
  /// 実行する最大ステート数（デフォルトはシリアル入出力の１バイト分に相当）
  /// @note
  /// 指定したステート数経過時に命令実行の途中であれば最大ステート数を超過する。
  uint64_t clock(const uint64_t maxStates = SerialUnitStates) {
    uint64_t states = 0;
    m_run = true;
    do {
      states += step();
    } while (states < maxStates && m_run);
    return states;
  }

  /// @brief シリアル入力バッファ満フラグの値を取得する。
  /// @return シリアル入力バッファ満フラグの値
  bool isSerialInFull() const noexcept { return m_rxFull; }

  /// @brief シリアル入力に1バイト書き込む。
  /// @param val 値
  /// @return 正常に書き込めた場合は true, そうでなければ false
  bool tryWriteSerialIn(const uint8_t val) noexcept {
    bool ok = false;
    if (not m_rxFull) {
      m_rxReg = val;
      m_rxFull = true;
      ok = true;
    }
    return ok;
  }

  /// @brief シリアル出力から1バイト読み取る。
  /// @return 読み取った値（読み取れなければ std::nullopt）
  std::optional<uint8_t> tryReadSerialOut() noexcept {
    std::optional<uint8_t> val = std::nullopt;
    if (not m_txEmpty) {
      val = m_txReg;
      m_txEmpty = true;
    }
    return val;
  }

  /// @brief プログラムを主記憶に書き込む。
  /// @param start 開始アドレス
  /// @param size サイズ
  /// @param values 値
  void writeProg(const uint8_t start, const uint8_t size,
                 const std::array<uint8_t, 256> &values) noexcept {
    for (uint16_t i = 0; i < static_cast<uint16_t>(size); ++i) {
      writeMem(static_cast<uint8_t>(start + i), values[i]);
    }
  }

  /// @brief コンソール割り込みを発生させる。
  void write() noexcept { m_int3 = true; }

  /// @brief パラレル出力の値を取得する。
  /// @return パラレル出力の値
  uint8_t readParallel() const noexcept { return m_parallelOut; }

  /// @brief 拡張パラレル出力の値を取得する。
  /// @return 拡張パラレル出力の値
  uint8_t readExtParallel() const noexcept { return m_extParallelOut; }

  /// @brief パラレル入力の値を設定する。
  /// @param val パラレル入力の値
  void writeParallel(const uint8_t val) noexcept {
    m_parallelIn = val;
    // HIGH: 3[V], LOW: 0[V] としたときの対応するアナログ値
    static constexpr uint8_t HighVal = static_cast<uint8_t>(255 * 3.0F / 3.3F);
    static constexpr uint8_t LowVal = 0;
    m_adcChs[0] = (val & 0x01) != 0 ? HighVal : LowVal;
    m_adcChs[1] = (val & 0x02) != 0 ? HighVal : LowVal;
    m_adcChs[2] = (val & 0x04) != 0 ? HighVal : LowVal;
    m_adcChs[3] = (val & 0x08) != 0 ? HighVal : LowVal;
  }

  /// @brief アナログ入力の値を設定する。
  /// @param pin ピン番号 (0 ~ 3)
  /// @param val アナログ入力の値
  void writeAnalog(const uint8_t pin, const uint8_t val) noexcept {
    assert(pin < m_adcChs.size());
    m_adcChs[pin] = val;
    // 1.6[V] を超えると 1 とする
    static constexpr uint8_t Threshold =
        static_cast<uint8_t>(255 * 1.6F / 3.3F);
    m_parallelIn =
        (m_parallelIn & ~(1 << pin)) | ((Threshold < val ? 1 : 0) << pin);
  }

private:
  // レジスタ
  /// @brief G0
  uint8_t m_g0;
  /// @brief G1
  uint8_t m_g1;
  /// @brief G2
  uint8_t m_g2;
  /// @brief SP
  uint8_t m_sp;
  /// @brief PC
  uint8_t m_pc;

  // フラグ
  /// @brief C
  bool m_c : 1;
  /// @brief S
  bool m_s : 1;
  /// @brief Z
  bool m_z : 1;
  /// @brief 割り込み許可
  bool m_intEna : 1;
  /// @brief 実行フラグ
  bool m_run : 1;
  /// @brief エラーフラグ
  bool m_err : 1;

  /// @brief 主記憶
  std::array<uint8_t, 256> m_mm;

  // 入出力
  /// @brief データスイッチ
  uint8_t m_dataSW;
  /// @brief シリアル入力レジスタ
  uint8_t m_rxReg;
  /// @brief シリアル出力レジスタ
  uint8_t m_txReg;
  /// @brief タイマカウンタ
  uint8_t m_tmrCnt;
  /// @brief タイマ周期レジスタ
  uint8_t m_tmrPeriod;
  /// @brief パラレル入力レジスタ
  uint8_t m_parallelIn;
  /// @brief パラレル出力レジスタ
  uint8_t m_parallelOut;
  /// @brief パラレル出力レジスタ（拡張）
  uint8_t m_extParallelOut : 4;
  /// @brief アナログ入力レジスタ
  std::array<uint8_t, 4> m_adcChs;
  /// @brief ブザー
  bool m_buz : 1;
  /// @brief スピーカ
  bool m_spk : 1;
  /// @brief シリアル出力送信バッファ空フラグ
  bool m_txEmpty : 1;
  /// @brief シリアル入力受信バッファ満フラグ
  bool m_rxFull : 1;
  /// @brief シリアル送信割り込み有効フラグ
  bool m_txIntEna : 1;
  /// @brief シリアル受信割り込み有効フラグ
  bool m_rxIntEna : 1;
  /// @brief タイマ有効フラグ
  bool m_tmrEna : 1;
  /// @brief タイマ割り込み有効フラグ
  bool m_tmrIntEna : 1;
  /// @brief コンソール割り込み有効フラグ
  bool m_cslIntEna : 1;
  /// @brief 拡張パラレル出力有効フラグ
  bool m_extParallelOutEna : 1;
  /// @brief タイマ経過フラグ
  bool m_tmrElapsed : 1;
  /// @brief タイマ割り込みフラグ
  bool m_int0 : 1;
  /// @brief コンソール割り込みフラグ
  bool m_int3 : 1;
  /// @brief タイマを動作させるためのカウンタ
  uint16_t m_tmrClkCnt;

  /// @brief タイマカウンタの増加させるステート数
  static constexpr uint16_t TmrClk = static_cast<uint16_t>(StatesPerSec / 75);
  /// @brief ROM領域（IPL）の開始アドレス
  static constexpr uint8_t RomStartAddr = 0xE0;
  /// @brief INT0（タイマ）割り込みベクタ
  static constexpr uint8_t Int0Vec = 0xDC;
  /// @brief INT1（SIO受信）割り込みベクタ
  static constexpr uint8_t Int1Vec = 0xDD;
  /// @brief INT2（SIO送信）割り込みベクタ
  static constexpr uint8_t Int2Vec = 0xDE;
  /// @brief INT3 (コンソール) 割り込みベクタ
  static constexpr uint8_t Int3Vec = 0xDF;

  /// @brief 主記憶へ値を書き込む。ただし、ROM領域には書き込まない。
  /// @param addr アドレス
  /// @param val 値
  void writeMem(const uint8_t addr, const uint8_t val) noexcept {
    if (addr < RomStartAddr) {
      m_mm[addr] = val;
    }
  }

  /// @brief 主記憶の値を読む。
  /// @param addr アドレス
  /// @return 値
  uint8_t readMem(const uint8_t addr) const noexcept { return m_mm[addr]; }

  /// @brief XRフィールドを考慮してアドレスを求める。
  /// @param xr XR
  /// @param addr アドレス
  /// @return アドレス
  uint8_t calcAddr(const uint8_t xr, const uint8_t addr) const noexcept {
    uint8_t a = 0x00;
    switch (xr) {
    case 0b00: // ダイレクト
      a = addr;
      break;
    case 0b01: // G1インデクスド
      a = static_cast<uint8_t>(addr + m_g1);
      break;
    case 0b10: // G2インデクスド
      a = static_cast<uint8_t>(addr + m_g2);
      break;
    default: // 即値（この関数では求められない）
      BUG("TeC::calcAddr(uint8_t, uint8_t) const noexcept");
      break;
    }
    return a;
  }

  /// @brief XRフィールドを考慮して主記憶を読む。
  /// @param xr XR
  /// @param addr アドレス
  /// @return 値
  uint8_t readMem(const uint8_t xr, const uint8_t addr) const noexcept {
    uint8_t val = 0x00;
    switch (xr) {
    case 0b00: // ダイレクト
      val = readMem(addr);
      break;
    case 0b01: // G1インデクスド
      val = readMem(static_cast<uint8_t>(addr + m_g1));
      break;
    case 0b10: // G2インデクスド
      val = readMem(static_cast<uint8_t>(addr + m_g2));
      break;
    case 0b11: // 即値
      val = addr;
      break;
    default:
      BUG("TeC::readMem(uint8_t, uint8_t) const noexcept");
      break;
    }
    return val;
  }

  /// @brief レジスタに値を書き込む。
  /// @param gr レジスタ
  /// @param val 値
  void writeReg(const uint8_t gr, const uint8_t val) noexcept {
    switch (gr) {
    case 0b00: // G0
      m_g0 = val;
      break;
    case 0b01: // G1
      m_g1 = val;
      break;
    case 0b10: // G2
      m_g2 = val;
      break;
    case 0b11: // SP
      m_sp = val;
      break;
    default:
      BUG("TeC::writeReg(uint8_t, uint8_t) noexcept");
      break;
    }
  }

  /// @brief レジスタの値を読む。
  /// @param gr レジスタ
  /// @return 値
  uint8_t readReg(const uint8_t gr) const noexcept {
    uint8_t v = 0x00;
    switch (gr) {
    case 0b00: // G0
      v = m_g0;
      break;
    case 0b01: // G1
      v = m_g1;
      break;
    case 0b10: // G2
      v = m_g2;
      break;
    case 0b11: // SP
      v = m_sp;
      break;
    default:
      BUG("TeC::readReg(uint8_t) const noexcept");
      break;
    }
    return v;
  }

  /// @brief
  /// 主記憶のプログラムカウンタのアドレスが指す値を読み、プログラムカウンタを１増やす。
  /// @return 読み取った値
  uint8_t fetch() noexcept { return readMem(m_pc++); }

  /// @brief エラーフラグを1に、実行フラグを0にする。
  void error() noexcept {
    m_err = true;
    m_run = false;
  }

  /// @brief 指定した割り込みベクタで割り込みを発生させる。
  /// @param vec 割り込みベクタ
  void interrupt(const uint8_t vec) noexcept {
    // PCをスタックに退避
    writeMem(--m_sp, m_pc);
    // フラグをスタックに退避
    writeMem(--m_sp, static_cast<uint8_t>(
                         (m_intEna ? 0x80 : 0x00) | (m_c ? 0x04 : 0x00) |
                         (m_s ? 0x02 : 0x00) | (m_z ? 0x01 : 0x00)));
    // プログラムカウンタの値を割り込みベクタに設定
    m_pc = readMem(vec);
    // 割り込みを無効化
    m_intEna = false;
  }

  /// @brief 1命令実行する。
  /// @return 実行に要したステート数（エラーの場合は0）
  uint8_t step() noexcept {
    // タイマ
    if (m_tmrEna) {
      if (TmrClk <= m_tmrClkCnt) {
        m_tmrClkCnt = 0;
        if (m_tmrCnt == m_tmrPeriod) {
          m_tmrCnt = 0;
          m_tmrElapsed = true;
          if (m_tmrIntEna) {
            m_int0 = true;
          }
        } else {
          ++m_tmrCnt;
        }
      }
    }
    // 割り込み
    if (m_intEna) {
      if (m_tmrIntEna && m_int0) {
        m_int0 = false; // INT0（タイマ割り込み）をリセット
        interrupt(Int0Vec);
      } else if (m_rxIntEna && m_rxFull) {
        interrupt(Int1Vec);
      } else if (m_txIntEna && m_txEmpty) {
        interrupt(Int2Vec);
      } else if (m_cslIntEna && m_int3) {
        m_int3 = false; // INT3（コンソール割り込み）をリセット
        interrupt(Int3Vec);
      }
    }
    const uint8_t inst = fetch();
    const uint8_t op = static_cast<uint8_t>((inst >> 4) & 0x0F);
    const uint8_t gr = static_cast<uint8_t>((inst >> 2) & 0x03);
    const uint8_t xr = static_cast<uint8_t>(inst & 0x03);
    uint8_t states = 0;
    switch (op) {
    case 0x0: // NO
      if (gr != 0b00 || xr != 0b00) {
        error();
      } else {
        states += 2;
      }
      break;
    case 0x1: // LD
      writeReg(gr, readMem(xr, fetch()));
      states += 4;
      break;
    case 0x2: // ST
      switch (xr) {
      case 0b00:
        writeMem(fetch(), readReg(gr));
        states += 3;
        break;
      case 0b01:
        writeMem(static_cast<uint8_t>(fetch() + m_g1), readReg(gr));
        states += 3;
        break;
      case 0b10:
        writeMem(static_cast<uint8_t>(fetch() + m_g2), readReg(gr));
        states += 3;
        break;
      case 0b11:
        error();
        break;
      }
      break;
    case 0x3: { // ADD
      const uint16_t val = static_cast<uint16_t>(readReg(gr)) +
                           static_cast<uint16_t>(readMem(xr, fetch()));
      m_c = (val & 0x100) != 0;
      m_s = (val & 0x080) != 0;
      m_z = (val & 0x0FF) == 0;
      writeReg(gr, static_cast<uint8_t>(val & 0xFF));
      states += 4;
    } break;
    case 0x4: { // SUB
      const uint16_t val = static_cast<uint16_t>(readReg(gr)) -
                           static_cast<uint16_t>(readMem(xr, fetch()));
      m_c = (val & 0x100) != 0;
      m_s = (val & 0x080) != 0;
      m_z = (val & 0x0FF) == 0;
      writeReg(gr, static_cast<uint8_t>(val & 0xFF));
      states += 4;
    } break;
    case 0x5: { // CMP
      const uint16_t val = static_cast<uint16_t>(readReg(gr)) -
                           static_cast<uint16_t>(readMem(xr, fetch()));
      m_c = (val & 0x100) != 0;
      m_s = (val & 0x080) != 0;
      m_z = (val & 0x0FF) == 0;
      states += 4;
    } break;
    case 0x6: { // AND
      const uint8_t val = readReg(gr) & readMem(xr, fetch());
      m_c = false;
      m_s = (val & 0x80) != 0;
      m_z = val == 0;
      writeReg(gr, val);
      states += 4;
    } break;
    case 0x7: { // OR
      const uint8_t val = readReg(gr) | readMem(xr, fetch());
      m_c = false;
      m_s = (val & 0x80) != 0;
      m_z = val == 0;
      writeReg(gr, val);
      states += 4;
    } break;
    case 0x8: { // XOR
      const uint8_t val = readReg(gr) ^ readMem(xr, fetch());
      m_c = false;
      m_s = (val & 0x80) != 0;
      m_z = val == 0;
      writeReg(gr, val);
      states += 4;
    } break;
    case 0x9: { // Shift
      uint8_t val = readReg(gr);
      switch (xr) {
      case 0b00:
      case 0b01:
        m_c = (val & 0x80) != 0;
        val <<= 1;
        break;
      case 0b10:
        m_c = (val & 0x01) != 0;
        val = (val & 0x80) | (val >> 1);
        break;
      case 0b11:
        m_c = (val & 0x01) != 0;
        val = (val >> 1) & ~0x80;
        break;
      }
      m_s = (val & 0x80) != 0;
      m_z = val == 0;
      writeReg(gr, val);
      states += 3;
    } break;
    case 0xA: { // Jump 1
      if (xr == 0b11) {
        error();
      } else {
        bool jmp = false;
        switch (gr) {
        case 0b00: // JMP
          jmp = true;
          break;
        case 0b01:
          jmp = m_z;
          break;
        case 0b10:
          jmp = m_c;
          break;
        case 0b11:
          jmp = m_s;
          break;
        default:
          BUG("TeC::step() noexcept");
          break;
        }
        const uint8_t addr = calcAddr(xr, fetch());
        if (jmp) {
          m_pc = addr;
        }
        states += 3;
      }
    } break;
    case 0xB: // Jump 2
    {
      if (xr == 0b11) {
        error();
      } else {
        const uint8_t addr = calcAddr(xr, fetch());
        bool jmp;
        switch (gr) {
        case 0b00: // CALL
          jmp = true;
          writeMem(--m_sp, m_pc);
          ++states;
          break;
        case 0b01:
          jmp = not m_z;
          break;
        case 0b10:
          jmp = not m_c;
          break;
        case 0b11:
          jmp = not m_s;
          break;
        }
        if (jmp) {
          m_pc = addr;
        }
        states += 3;
      }
    } break;
    case 0xC:
      switch (xr) {
      case 0b00: { // IN
        const uint8_t addr = fetch();
        if (addr < 0x10) {
          uint8_t val;
          switch (addr) {
          case 0x0: // Data-Sw
          case 0x1: // Data-Sw
            val = m_dataSW;
            break;
          case 0x2: // SIO-DATA
            val = m_rxReg;
            m_rxFull = false;
            break;
          case 0x3: // SIO-STAT
            val = static_cast<uint8_t>((m_rxFull ? 0x40 : 0x00) |
                                       (m_txEmpty ? 0x80 : 0x00));
            break;
          case 0x4: // TMR現在値
            val = m_tmrCnt;
            break;
          case 0x5: // TMR-Stat
            val = m_tmrElapsed ? 0x80 : 0x00;
            m_tmrElapsed = false;
            break;
          case 0x7:
            val = m_parallelIn;
            break;
          case 0x8:
          case 0x9:
          case 0xA:
          case 0xB:
            val = m_adcChs[addr - 0x8];
            break;
          case 0x6:
          case 0xC:
          case 0xD:
          case 0xE:
          case 0xF:
            val = 0x00;
            break;
          default:
            BUG("TeC::step() noexcept");
            break;
          }
          writeReg(gr, val);
          states += 4;
        } else {
          error();
        }
      } break;
      case 0b11: { // OUT
        const uint8_t addr = fetch();
        if (addr < 0x10) {
          const uint8_t val = readReg(gr);
          switch (addr) {
          case 0x0: // BUZ
            m_buz = (val & 0x01) != 0;
            break;
          case 0x1: // SPK
            m_spk = (val & 0x01) != 0;
            break;
          case 0x2: // SIO-DATA
            m_txReg = val;
            m_txEmpty = false;
            break;
          case 0x3: // SIO-CTRL
            m_txIntEna = (val & 0x80) != 0;
            m_rxIntEna = (val & 0x40) != 0;
            break;
          case 0x4: // TMR周期
            m_tmrPeriod = val;
            break;
          case 0x5: // TMR-CTRL
            m_tmrIntEna = (val & 0x80) != 0;
            if ((m_tmrEna = (val & 0x01) != 0)) {
              m_tmrElapsed = false;
              // タイマ開始時にカウンタをリセット
              m_tmrCnt = 0x00;
            }
            break;
          case 0x6: // Console STI
            m_cslIntEna = (val & 0x01) != 0;
            break;
          case 0x7: // PIO-OUTPUT
            m_parallelOut = val;
            break;
          case 0xC: // PIO-Ctrl
            if ((m_extParallelOutEna = (val & 0x80) != 0)) {
              m_extParallelOut = val & 0x0F;
            }
            break;
          case 0x8:
          case 0x9:
          case 0xA:
          case 0xB:
          case 0xD:
          case 0xE:
          case 0xF:
            break;
          default:
            BUG("TeC::step() noexcept");
            break;
          }
          states += 3;
        } else {
          error();
        }
      } break;
      default:
        error();
        break;
      }
      break;
    case 0xD:
      switch (xr) {
      case 0b00:
        writeMem(m_sp - 1, readReg(gr));
        --m_sp;
        states += 3;
        break;
      case 0b10:
        writeReg(gr, readMem(m_sp));
        ++m_sp;
        states += 4;
        break;
      default:
        error();
        break;
      }
      break;
    case 0xE:
      switch (gr) {
      case 0b00:
        switch (xr) {
        case 0b00: // EI
          m_intEna = true;
          states += 3;
          break;
        case 0b11: // DI
          m_intEna = false;
          states += 3;
          break;
        default:
          error();
          break;
        }
        break;
      case 0b11:
        switch (xr) {
        case 0b00: // RET
          m_pc = readMem(m_sp++);
          states += 3;
          break;
        case 0b11: { // RETI
          const uint8_t flg = readMem(m_sp++);
          m_intEna = (flg & 0x80) != 0;
          m_c = (flg & 0x04) != 0;
          m_s = (flg & 0x02) != 0;
          m_z = (flg & 0x01) != 0;
          m_pc = readMem(m_sp++);
          states += 4;
        } break;
        default:
          error();
          break;
        }
        break;
      default:
        error();
        break;
      }
      break;
    case 0xF:
      if (gr == 0b11 && xr == 0b11) {
        m_run = false;
      } else {
        error();
      }
      break;
    }
    // 実行したステート数（= クロック数）をカウント
    m_tmrClkCnt += states;
    return states;
  }
};

/// @brief 命令のタイプ
enum class EventType : uint8_t {
  /// @brief レジスタへの書き込み
  SetReg,
  /// @brief フラグへの書き込み
  SetFlg,
  /// @brief メモリへの書き込み
  SetMM,
  /// @brief データスイッチの設定
  SetDataSW,
  /// @brief 実行開始
  Run,
  /// @brief 実行停止
  Stop,
  /// @brief シリアル入力への書き込み
  Serial,
  /// @brief （前回のイベントから）一定のステート数以上待機
  WaitStates,
  /// @brief シリアル入力が全て受け取られるまで待機
  WaitSerial,
  /// @brief 実行停止まで待機
  WaitStop,
  /// @brief コンソール割り込みの発生
  Write,
  /// @brief リセット
  Reset,
  /// @brief レジスタの出力
  PrintReg,
  /// @brief フラグの出力
  PrintFlg,
  /// @brief メモリの出力
  PrintMM,
  /// @brief ブザーの出力
  PrintBuz,
  /// @brief スピーカの出力
  PrintSpk,
  /// @brief RUNランプの出力
  PrintRun,
  /// @brief シリアルモード
  SetSerialMode,
  /// @brief プリントモード
  SetPrintMode,
  /// @brief アナログ入力
  Analog,
  /// @brief パラレル入力への書き込み
  ParallelWrite,
  /// @brief パラレル出力の読み取り
  PrintParallel,
  /// @brief 拡張パラレル出力の読み取り
  PrintExtParallel,
};

/// @brief 出力モード
enum class OutputMode : uint8_t {
  /// @brief そのまま
  Raw,
  /// @brief 16進数 (XX) 1オクテットごとに空白, 8オクテットで改行
  Hex,
  /// @brief TeC形式 (0XXH) 1オクテットごとに改行
  TeC,
  /// @brief 符号付き10進 1オクテットごとに改行
  SDEC,
  /// @brief 符号なし10進 1オクテットごとに改行
  UDEC
};

[[nodiscard]] static inline std::optional<OutputMode>
StrToOutputMode(const std::string &s) {
  std::optional<OutputMode> mode = std::nullopt;
  if (s == "RAW") {
    mode = OutputMode::Raw;
  } else if (s == "HEX") {
    mode = OutputMode::Hex;
  } else if (s == "TEC") {
    mode = OutputMode::TeC;
  } else if (s == "SDEC") {
    mode = OutputMode::SDEC;
  } else if (s == "UDEC") {
    mode = OutputMode::UDEC;
  }
  return mode;
}

/// @brief シリアル出力モード
using SerialMode = OutputMode;

/// @brief 表示モード
using PrintMode = OutputMode;

/// @brief シリアル出力モードの初期値
static constexpr SerialMode DefaultSerialMode = SerialMode::Raw;

/// @brief 表示モードの初期値
static constexpr PrintMode DefaultPrintMode = PrintMode::UDEC;

/// @brief イベント処理
struct Event {
  Event(const EventType type) noexcept : type(type) {}

  virtual ~Event() = default;

  EventType type;
};

struct PrintFlgEvent : public Event {
  PrintFlgEvent(const Flg flg) noexcept
      : Event(EventType::PrintFlg), flg(flg) {}

  Flg flg;
};

struct PrintRegEvent : public Event {
  PrintRegEvent(const Reg reg) noexcept
      : Event(EventType::PrintReg), reg(reg) {}

  Reg reg;
};

struct PrintMMEvent : public Event {
  PrintMMEvent(const uint8_t addr) noexcept
      : Event(EventType::PrintMM), addr(addr) {}

  uint8_t addr;
};

struct SetRegEvent : public Event {
  SetRegEvent(const Reg reg, uint8_t value) noexcept
      : Event(EventType::SetReg), reg(reg), value(value) {}

  Reg reg;
  uint8_t value;
};

struct SetFlgEvent : public Event {
  SetFlgEvent(const Flg flg, const bool val) noexcept
      : Event(EventType::SetFlg), flg(flg), val(val) {}

  Flg flg;
  bool val;
};

struct SetMMEvent : public Event {
  SetMMEvent(const uint8_t addr, const uint8_t val) noexcept
      : Event(EventType::SetMM), addr(addr), val(val) {}

  uint8_t addr;
  uint8_t val;
};

struct SetDataSWEvent : public Event {
  SetDataSWEvent(const uint8_t val) noexcept
      : Event(EventType::SetDataSW), val(val) {}
  uint8_t val;
};

struct SetSerialModeEvent : public Event {
  SetSerialModeEvent(const SerialMode mode) noexcept
      : Event(EventType::SetSerialMode), mode(mode) {}

  SerialMode mode;
};

struct SetPrintModeEvent : public Event {
  SetPrintModeEvent(const PrintMode mode) noexcept
      : Event(EventType::SetPrintMode), mode(mode) {}

  SerialMode mode;
};

struct SerialEvent : public Event {
  SerialEvent(std::vector<uint8_t> &&value)
      : Event(EventType::Serial), value(std::move(value)) {}

  std::vector<uint8_t> value;
};

struct WaitStatesEvent : public Event {
  WaitStatesEvent(const uint64_t states) noexcept
      : Event(EventType::WaitStates), states(states) {}

  uint64_t states;
};

struct AnalogEvent : public Event {
  AnalogEvent(const uint8_t pin, const uint8_t value) noexcept
      : Event(EventType::Analog), pin(pin), value(value) {}

  uint8_t pin;
  uint8_t value;
};

struct ParallelWriteEvent : public Event {
  ParallelWriteEvent(const uint8_t value) noexcept
      : Event(EventType::ParallelWrite), value(value) {}

  uint8_t value;
};

using EventList = std::vector<std::unique_ptr<Event>>;

/// @brief 標準入力を読み取り、イベント処理リストを返す。
/// @param nameTable 名前表
/// @return イベント処理リスト
static inline EventList ReadInput(const NameTable &nameTable) {
  // 入力読み取り用
  struct InputReader {
    // 名前表
    const NameTable &nameTable;
    // 現在の行
    std::string curLine = "";
    // 現在の文字の添え字
    size_t curIdx = 0;
    // 空白とコメントを読み飛ばす。
    void skipSpaceOrComment() {
      while (curIdx < curLine.size()) {
        if (std::isspace(curLine[curIdx])) {
          ++curIdx;
        } else if (curLine[curIdx] == ';') {
          curIdx = curLine.size();
          break;
        } else {
          break;
        }
      }
    }
    // ラベルの開始文字を判定する。
    [[nodiscard]] bool isLabelStart() {
      return curIdx < curLine.size() &&
             (std::isalpha(curLine[curIdx]) || curLine[curIdx] == '_');
    }
    // ラベル文字を判定する。
    [[nodiscard]] bool isLabel() {
      return curIdx < curLine.size() &&
             (std::isalnum(curLine[curIdx]) || curLine[curIdx] == '_');
    }
    // 1文字判定する。
    [[nodiscard]] bool isCh(const char ch) {
      if (curIdx < curLine.size() && curLine[curIdx] == ch) {
        ++curIdx;
        return true;
      }
      return false;
    }
    // ラベルの値を読み取る。
    [[nodiscard]] bool getLabel(uint8_t &val) {
      assert(isLabelStart());
      std::string label;
      do {
        label += std::toupper(curLine[curIdx++]);
      } while (isLabel());
      val = 0x00;
      if (const auto nameTableIt = nameTable.find(label);
          nameTableIt != nameTable.cend()) {
        val = nameTableIt->second;
      } else {
        PrintError(
            std::format("ラベルが見つかりません。 (ラベル: \"{}\")", label),
            ErrorType::Program);
        return false;
      }
      return true;
    }
    // 10進数文字を判定する。
    [[nodiscard]] bool isDigit() {
      return curIdx < curLine.size() && std::isdigit(curLine[curIdx]);
    }
    // 16進数文字を判定する。
    [[nodiscard]] bool isXDigit() {
      return curIdx < curLine.size() && std::isxdigit(curLine[curIdx]);
    }
    // 数字を読む。
    [[nodiscard]] bool getNum(uint8_t &val) {
      assert(isDigit());
      std::string numStr;
      bool isHex = false;
      do {
        if (not isDigit()) {
          isHex = true;
        }
        numStr += curLine[curIdx++];
      } while (isXDigit());
      if (isCh('H') || isCh('h')) {
        isHex = true;
      } else if (isHex) {
        PrintError("16進数リテラルが不正です。（'H' が必要です。）",
                   ErrorType::Input);
        return false;
      }
      val = 0x00;
      try {
        size_t lastIdx;
        val =
            static_cast<uint8_t>(std::stoi(numStr, &lastIdx, isHex ? 16 : 10));
        if (lastIdx != numStr.size()) {
          BUG("stoi");
          return false;
        }
      } catch (const std::invalid_argument &e) {
        BUG("stoi");
        return false;
      } catch (const std::out_of_range &e) {
        PrintError(std::format("値が大きすぎます。 (値: \"{}\")", numStr),
                   ErrorType::Input);
        return false;
      }
      return true;
    }
    // 値を読む。
    [[nodiscard]] bool getValue(uint8_t &val) {
      // 空白を読み飛ばす。
      skipSpaceOrComment();
      bool pos = true;
      if (isCh('+')) { // 正
        skipSpaceOrComment();
      } else if (isCh('-')) { // 負
        skipSpaceOrComment();
        pos = false;
      }

      if (isLabelStart()) { // ラベル
        if (not getLabel(val)) {
          return false;
        }
      } else if (isDigit()) { // 数字
        if (not getNum(val)) {
          return false;
        }
      } else if (isCh('(')) { // 括弧
        if (not getAdd(val)) {
          return false;
        }
        skipSpaceOrComment();
        if (not isCh(')')) {
          PrintError("')' が必要です。", ErrorType::Input);
          return false;
        }
      } else if (isCh('\'')) { // 文字定数
        if (curLine.size() <= curIdx || not std::isprint(curLine[curIdx])) {
          PrintError("文字定数が不正です。", ErrorType::Input);
          return false;
        }
        val = static_cast<uint8_t>(curLine[curIdx++]);
        if (not isCh('\'')) {
          PrintError("'\\'' （クォーテーション）が必要です。",
                     ErrorType::Input);
          return false;
        }
      } else { // エラー
        PrintError("値が必要です。", ErrorType::Input);
        return false;
      }
      if (not pos) {
        val = static_cast<uint8_t>(~val + 1);
      }
      return true;
    }
    // 乗除算を読む。
    [[nodiscard]] bool getMul(uint8_t &val) {
      if (not getValue(val)) {
        return false;
      }
      for (;;) {
        skipSpaceOrComment();
        if (isCh('*')) {
          uint8_t rVal = 0x00;
          if (not getValue(rVal)) {
            return false;
          }
          val *= rVal;
        } else if (isCh('/')) {
          uint8_t rVal = 0x00;
          if (not getValue(rVal)) {
            return false;
          }
          if (rVal == 0) {
            PrintError("零除算が検出されました。", ErrorType::Input);
            return false;
          }
          val /= rVal;
        } else {
          break;
        }
      }
      return true;
    }
    // 加減算を読む。
    [[nodiscard]] bool getAdd(uint8_t &val) {
      if (not getMul(val)) {
        return false;
      }
      for (;;) {
        skipSpaceOrComment();
        if (isCh('+')) {
          uint8_t rVal = 0x00;
          if (not getMul(rVal)) {
            return false;
          }
          val += rVal;
        } else if (isCh('-')) {
          uint8_t rVal = 0x00;
          if (not getMul(rVal)) {
            return false;
          }
          val -= rVal;
        } else {
          break;
        }
      }
      return true;
    }
    // コマンドやその引数の開始文字を判定する。
    [[nodiscard]] bool isWordStart() {
      return curIdx < curLine.size() &&
             (std::isalpha(curLine[curIdx]) || curLine[curIdx] == '_');
    }
    // コマンドやその引数に使う文字を判定する。
    [[nodiscard]] bool isWord() {
      return curIdx < curLine.size() &&
             (std::isalnum(curLine[curIdx]) || curLine[curIdx] == '-' ||
              curLine[curIdx] == '_');
    }
    // '=' があるか調べる。
    [[nodiscard]] bool checkEQ() {
      skipSpaceOrComment();
      if (not isCh('=')) {
        PrintError("'=' が必要です。", ErrorType::Input);
        return false;
      }
      return true;
    }
    // ']' があるか調べる。
    [[nodiscard]] bool checkRSP() {
      skipSpaceOrComment();
      if (not isCh(']')) {
        PrintError("']' が必要です。", ErrorType::Input);
        return false;
      }
      return true;
    }
    // コマンドやその引数を取得する。
    [[nodiscard]] bool getWord(std::string &word) {
      skipSpaceOrComment();
      if (not isWordStart()) {
        return false;
      }
      do {
        word += std::toupper(curLine[curIdx++]);
      } while (isWord());
      return true;
    }
    // 実数を読む
    [[nodiscard]] bool getFloat(float &val) {
      skipSpaceOrComment();
      if (not isDigit()) {
        PrintError("実数が必要です。", ErrorType::Input);
        return false;
      }
      std::string numStr;
      do {
        numStr += curLine[curIdx++];
      } while (isDigit());
      if (isCh('.')) {
        if (not isDigit()) {
          PrintError("'.' の後に小数部がありません。", ErrorType::Input);
          return false;
        }
        numStr += '.';
        do {
          numStr += curLine[curIdx++];
        } while (isDigit());
      }
      try {
        size_t lastIdx;
        val = std::stof(numStr, &lastIdx);
        if (lastIdx != numStr.size()) {
          BUG("stoi");
          return false;
        }
      } catch (const std::invalid_argument &e) {
        BUG("stoi");
        return false;
      } catch (const std::out_of_range &e) {
        PrintError(std::format("実数が大きすぎます。 （実数: \"{}\"）", numStr),
                   ErrorType::Input);
        return false;
      }
      return true;
    }
    // 一行読む
    [[nodiscard]] bool readLine(EventList &eventList) {
      if (isCh('$')) { // コマンド行
        std::string cmd;
        if (not getWord(cmd)) {
          PrintError("コマンドが必要です。", ErrorType::Input);
          return true;
        }
        if (cmd == "RUN") {
          eventList.emplace_back(std::make_unique<Event>(EventType::Run));
        } else if (cmd == "STOP") {
          eventList.emplace_back(std::make_unique<Event>(EventType::Stop));
        } else if (cmd == "RESET") {
          eventList.emplace_back(std::make_unique<Event>(EventType::Reset));
        } else if (cmd == "WAIT") {
          std::string arg;
          if (not getWord(arg)) {
            PrintError("引数が必要です。", ErrorType::Input);
            return true;
          }
          if (arg == "STOP") {
            eventList.emplace_back(
                std::make_unique<Event>(EventType::WaitStop));
          } else if (arg == "STATES" || arg == "MS" || arg == "SEC") {
            skipSpaceOrComment();
            if (curLine.size() <= curIdx || not std::isdigit(curLine[curIdx])) {
              PrintError("整数が必要です。", ErrorType::Input);
              return true;
            }
            std::string numStr;
            do {
              numStr += curLine[curIdx++];
            } while (curIdx < curLine.size() && std::isdigit(curLine[curIdx]));
            try {
              size_t lastIdx;
              uint64_t states =
                  static_cast<uint64_t>(std::stoull(numStr, &lastIdx, 10));
              if (lastIdx != numStr.size()) {
                BUG("stoull");
                return true;
              }
              if (arg == "MS") {
                states = states * TeC::StatesPerSec / 1000;
              } else if (arg == "SEC") {
                states = states * TeC::StatesPerSec;
              }
              eventList.emplace_back(std::make_unique<WaitStatesEvent>(
                  static_cast<uint64_t>(states)));
            } catch (const std::invalid_argument &e) {
              BUG("stoull");
              return true;
            } catch (const std::out_of_range &e) {
              PrintError(std::format("整数が大きすぎます。"
                                     "（整数: {}）",
                                     numStr),
                         ErrorType::Input);
              return true;
            }
          } else if (arg == "SERIAL") {
            eventList.emplace_back(
                std::make_unique<Event>(EventType::WaitSerial));
          } else {
            PrintError(std::format("WAITコマンドの対象が不正です。"
                                   "（対象: {}）",
                                   arg),
                       ErrorType::Input);
            return true;
          }
        } else if (cmd == "DATA-SW") {
          uint8_t val = 0x00;
          if (not getAdd(val)) {
            return true;
          }
          eventList.emplace_back(std::make_unique<SetDataSWEvent>(val));
        } else if (cmd == "SERIAL-MODE" || cmd == "PRINT-MODE") {
          std::string mode;
          if (not getWord(mode)) {
            PrintError("引数が必要です。", ErrorType::Input);
            return true;
          }
          if (const std::optional<OutputMode> m = StrToOutputMode(mode)) {
            if (cmd == "SERIAL-MODE") {
              eventList.emplace_back(
                  std::make_unique<SetSerialModeEvent>(m.value()));
            } else {
              eventList.emplace_back(
                  std::make_unique<SetPrintModeEvent>(m.value()));
            }
          } else {
            PrintError("出力モードが必要です。"
                       "（使用可能な出力モード: (RAW|HEX|TEC|SDEC|UDEC)）",
                       ErrorType::Input);
            return true;
          }
        } else if (cmd == "PRINT") {
          skipSpaceOrComment();
          if (isCh('[')) {
            uint8_t addr = 0x00;
            if (not getAdd(addr)) {
              return true;
            }
            if (not checkRSP()) {
              return true;
            }
            eventList.emplace_back(std::make_unique<PrintMMEvent>(addr));
          } else if (curIdx < curLine.size() && std::isalpha(curLine[curIdx])) {
            std::string regOrFlg;
            do {
              regOrFlg += std::toupper(curLine[curIdx++]);
            } while (curIdx < curLine.size() &&
                     (std::isalnum(curLine[curIdx]) || curLine[curIdx] == '-'));
            if (const std::optional<Reg> reg = StrToReg(regOrFlg)) {
              eventList.emplace_back(
                  std::make_unique<PrintRegEvent>(reg.value()));
            } else if (const std::optional<Flg> flg = StrToFlg(regOrFlg)) {
              eventList.emplace_back(
                  std::make_unique<PrintFlgEvent>(flg.value()));
            } else if (regOrFlg == "PARALLEL") {
              eventList.emplace_back(
                  std::make_unique<Event>(EventType::PrintParallel));
            } else if (regOrFlg == "EXT-PARALLEL") {
              eventList.emplace_back(
                  std::make_unique<Event>(EventType::PrintExtParallel));
            } else if (regOrFlg == "BUZ") {
              eventList.emplace_back(
                  std::make_unique<Event>(EventType::PrintBuz));
            } else if (regOrFlg == "SPK") {
              eventList.emplace_back(
                  std::make_unique<Event>(EventType::PrintSpk));
            } else if (regOrFlg == "RUN") {
              eventList.emplace_back(
                  std::make_unique<Event>(EventType::PrintRun));
            } else {
              PrintError(std::format("レジスタまたはフラグ名が不正です。 "
                                     "(名前の開始部: \"{}\")",
                                     regOrFlg),
                         ErrorType::Input);
              return true;
            }
          } else {
            PrintError("表示対象が不正です。", ErrorType::Input);
            return true;
          }
        } else if (cmd == "SERIAL") {
          std::vector<uint8_t> data;
          do {
            skipSpaceOrComment();
            if (isCh('"')) {
              while (curIdx < curLine.size() && std::isprint(curLine[curIdx]) &&
                     curLine[curIdx] != '"') {
                data.emplace_back(static_cast<uint8_t>(curLine[curIdx++]));
              }
              if (not isCh('"')) {
                PrintError("\" が必要です。", ErrorType::Input);
                return true;
              }
            } else {
              if (not getAdd(data.emplace_back())) {
                return true;
              }
            }
          } while (isCh(','));
          eventList.emplace_back(
              std::make_unique<SerialEvent>(std::move(data)));
        } else if (cmd == "WRITE") {
          eventList.emplace_back(std::make_unique<Event>(EventType::Write));
        } else if (cmd == "ANALOG") {
          std::string chStr;
          if (not getWord(chStr)) {
            PrintError("ADCチャンネルが必要です。", ErrorType::Input);
            return true;
          }
          if (chStr.size() != 3 || chStr[0] != 'C' || chStr[1] != 'H' ||
              chStr[2] < '0' || '3' < chStr[2]) {
            PrintError("ADCチャンネルが必要です。", ErrorType::Input);
            return true;
          }
          const uint8_t ch = static_cast<uint8_t>(chStr[2] - '0');
          float fVal = 0.0;
          if (not getFloat(fVal)) {
            return true;
          }
          skipSpaceOrComment();
          uint8_t val = 0;
          if (isCh('V')) {
            val = static_cast<uint8_t>(
                std::min(255U, static_cast<unsigned int>(255 * fVal / 3.3F)));
          } else if (isCh('m') && isCh('V')) {
            val = static_cast<uint8_t>(std::min(
                255U, static_cast<unsigned int>(255 * fVal / 3300.0F)));
          } else {
            PrintError("'V' または \"mV\" が必要です。", ErrorType::Input);
            return true;
          }
          eventList.emplace_back(std::make_unique<AnalogEvent>(ch, val));
        } else if (cmd == "PARALLEL") {
          uint8_t val = 0;
          if (not getAdd(val)) {
            return true;
          }
          eventList.emplace_back(std::make_unique<ParallelWriteEvent>(val));
        } else if (cmd == "END") {
          return false;
        } else {
          PrintError(
              std::format("不正なコマンドです。（コマンド名: \"{}\"）", cmd),
              ErrorType::Input);
          return true;
        }
      } else if (isCh('[')) { // 主記憶の値の変更
        uint8_t addr;
        if (not getAdd(addr)) {
          return true;
        }
        // ']'
        if (not checkRSP()) {
          return true;
        }
        // '='
        if (not checkEQ()) {
          return true;
        }
        uint8_t val = 0x00;
        if (not getAdd(val)) {
          return true;
        }
        eventList.emplace_back(std::make_unique<SetMMEvent>(addr, val));
      } else if (curIdx < curLine.size() && std::isalpha(curLine[curIdx])) {
        // レジスタかフラグ
        std::string cmd;
        do {
          cmd += std::toupper(curLine[curIdx++]);
        } while (curIdx < curLine.size() && std::isalnum(curLine[curIdx]));
        if (const std::optional<Reg> reg = StrToReg(cmd)) {
          if (not checkEQ()) {
            return true;
          }
          uint8_t val = 0x00;
          if (not getAdd(val)) {
            return true;
          }
          eventList.emplace_back(
              std::make_unique<SetRegEvent>(reg.value(), val));
        } else if (const std::optional<Flg> flg = StrToFlg(cmd)) {
          if (not checkEQ()) {
            return true;
          }
          skipSpaceOrComment();
          bool v = false;
          if (curIdx < curLine.size()) {
            if (curLine[curIdx] == '0') {
              ++curIdx;
              v = false;
            } else if (curLine[curIdx] == '1') {
              ++curIdx;
              v = true;
            } else {
              PrintError("'0' または '1' が必要です。", ErrorType::Input);
              return true;
            }
          }
          eventList.emplace_back(std::make_unique<SetFlgEvent>(flg.value(), v));
        } else {
          PrintError(
              std::format(
                  "レジスタまたはフラグ名が不正です。（名前の開始部: \"{}\"）",
                  cmd),
              ErrorType::Input);
          return true;
        }
      }
      skipSpaceOrComment();
      if (curIdx < curLine.size()) {
        PrintError(std::format("入力の後部が解析できませんでした。（行: {}）",
                               curLine),
                   ErrorType::Input);
        return true;
      }
      return true;
    }

    EventList operator()() {
      EventList eventList;
      while (std::getline(std::cin, curLine)) {
        curIdx = 0;
        if (not readLine(eventList)) {
          break;
        }
      }
      // 入力時点でエラーが発生すれば終了
      CheckError();
      // プログラム終了まで実行するため
      eventList.emplace_back(std::make_unique<Event>(EventType::WaitStop));
      return eventList;
    }
  };
  return InputReader{.nameTable = nameTable}();
}

/// @brief 名前表を読む。
/// @param path ファイルのパス
/// @return 名前表
static inline NameTable ReadNameTable(const char *path) {
  std::ifstream ifs{path};
  if (not ifs) {
    Error(std::format("ファイルが開けませんでした。"
                      "（ファイルのパス: \"{}\"）",
                      path),
          ErrorType::NameTable);
  }
  NameTable table;
  std::string line;
  size_t lineNum = 0;
  while (std::getline(ifs, line)) {
    size_t idx = 0;
    ++lineNum;
    // 空白を読み飛ばす。
    auto skipSpace = [&]() -> void {
      while (idx < line.size() && std::isspace(line[idx])) {
        ++idx;
      }
    };
    // ラベルの先頭文字判定
    auto isLabelStart = [&]() -> bool {
      return idx < line.size() && (std::isalpha(line[idx]) || line[idx] == '_');
    };
    // ラベル文字判定
    auto isLabel = [&]() -> bool {
      return idx < line.size() && (std::isalnum(line[idx]) || line[idx] == '_');
    };
    // 名前表のエラーを出力する。
    auto printNameTableError = [&](const std::string &msg) -> void {
      PrintError(std::format("{}:{}: {}", path, lineNum, msg),
                 ErrorType::NameTable);
    };
    skipSpace();
    if (idx < line.size()) {
      if (not isLabelStart()) {
        printNameTableError("ラベルが必要です。");
        continue;
      }
      std::string label;
      do {
        label += std::toupper(line[idx++]);
      } while (isLabel());
      skipSpace();
      if (line.size() <= idx || line[idx] != ':') {
        printNameTableError("':' が必要です。");
        continue;
      }
      ++idx;
      skipSpace();
      if (line.size() <= idx || not std::isdigit(line[idx])) {
        printNameTableError("値が必要です。");
        continue;
      }
      std::string numStr;
      bool hex = false;
      do {
        if (not std::isdigit(line[idx])) {
          hex = true;
        }
        numStr += line[idx++];
      } while (idx < line.size() && std::isxdigit(line[idx]));
      if (idx < line.size() && std::toupper(line[idx]) == 'H') {
        hex = true;
        ++idx;
      } else if (hex) {
        printNameTableError("'H' が必要です。");
        continue;
      }
      try {
        size_t lastIdx;
        uint8_t addr =
            static_cast<uint8_t>(std::stoi(numStr, &lastIdx, hex ? 16 : 10));
        if (lastIdx != numStr.size()) {
          BUG("stoi");
        }
        table.emplace(label, addr);
      } catch (const std::invalid_argument &e) {
        BUG("stoi");
      } catch (const std::out_of_range &e) {
        printNameTableError(
            std::format("値が大きすぎます。 （値: {}）", numStr));
        continue;
      }
      skipSpace();
      if (idx < line.size()) {
        printNameTableError(
            std::format("名前表の形式が不正です。（行: \"{}\"）", line));
        continue;
      }
    }
  }
  // エラーが発生していれば終了
  CheckError();
  return table;
}

/// @brief 入力されたバイナリ
struct Source {
  /// @brief 開始アドレス
  uint8_t start;
  /// @brief サイズ
  uint8_t size;
  /// @brief 値
  std::array<uint8_t, 256> values;
};

static inline Source readSource(const char *path) {
  std::ifstream ifs{path, std::ios_base::in | std::ios_base::binary};
  if (not ifs) {
    Error(std::format("ファイルが開けませんでした （ファイルのパス: \"{}\"）",
                      path),
          ErrorType::Binary);
  }
  Source source;
  source.start = static_cast<uint8_t>(ifs.get());
  if (ifs.eof()) {
    Error("機械語ファイルの形式が不正です。", ErrorType::Binary);
  }
  source.size = static_cast<uint8_t>(ifs.get());
  if (ifs.eof()) {
    Error("機械語ファイルの形式が不正です。", ErrorType::Binary);
  }
  ifs.read(reinterpret_cast<char *>(source.values.data()),
           sizeof(source.values));
  if (ifs.gcount() != source.size) {
    Error("機械語ファイルの形式が不正です。", ErrorType::Binary);
  }
  ifs.get();
  if (not ifs.eof()) {
    Error("機械語ファイルの形式が不正です。", ErrorType::Binary);
  }
  return source;
}

/// @brief 使用方法を出力して終了する。
/// @param cmd 自分自身の名前
[[noreturn]] static void Usage(const char *cmd) {
  std::cerr << std::format("使用方法: {} <program>.bin [<program>.nt]\n", cmd);
  std::exit(1);
}

/// @brief TeCのシリアル出力とその他の入出力の表示用
class Printer {
public:
  Printer()
      : m_serialMode(DefaultSerialMode), m_printMode(DefaultPrintMode),
        m_buffer(), m_curSrc(Src::None) {}

  void setSerialMode(const SerialMode mode) {
    if (m_curSrc == Src::Serial) {
      flush(m_serialMode);
    }
    m_serialMode = mode;
  }

  void setPrintMode(const PrintMode mode) {
    if (m_curSrc == Src::Print) {
      flush(m_printMode);
    }
    m_printMode = mode;
  }

  void serial(const uint8_t b) {
    if (m_curSrc != Src::Serial) {
      flush();
      m_curSrc = Src::Serial;
    }
    m_buffer.emplace_back(b);
  }

  void print(const uint8_t b) {
    if (m_curSrc != Src::Print) {
      flush();
      m_curSrc = Src::Print;
    }
    m_buffer.emplace_back(b);
  }

  void flush() {
    switch (m_curSrc) {
    case Src::None:
      assert(m_buffer.empty());
      break;
    case Src::Serial:
      flush(m_serialMode);
      break;
    case Src::Print:
      flush(m_printMode);
      break;
    default:
      BUG("Printer::flush");
      break;
    }
  }

private:
  SerialMode m_serialMode;

  PrintMode m_printMode;

  std::vector<uint8_t> m_buffer;

  enum class Src : uint8_t { None, Serial, Print } m_curSrc;

  void flush(const OutputMode mode) {
    switch (mode) {
    case SerialMode::Raw:
      for (const uint8_t ch : m_buffer) {
        std::cout << static_cast<char>(ch);
      }
      break;
    case SerialMode::Hex:
      for (size_t idx = 0; idx < m_buffer.size(); ++idx) {
        std::cout << std::format("{:0>2X}",
                                 static_cast<unsigned int>(m_buffer[idx]));
        if (idx + 1 < m_buffer.size()) {
          std::cout << (((idx + 1) & 7) == 0 ? '\n' : ' ');
        }
      }
      std::cout << '\n';
      break;
    case SerialMode::TeC:
      for (const uint8_t ch : m_buffer) {
        std::cout << std::format("{:0>3X}H\n", static_cast<unsigned int>(ch));
      }
      break;
    case SerialMode::SDEC:
      for (const uint8_t ch : m_buffer) {
        std::cout << std::format(
            "{}\n", static_cast<signed int>(static_cast<int8_t>(ch)));
      }
      break;
    case SerialMode::UDEC:
      for (const uint8_t ch : m_buffer) {
        std::cout << std::format("{}\n", static_cast<unsigned int>(ch & 0xFF));
      }
      break;
    default:
      BUG("Printer::flush");
      break;
    }
    m_buffer.clear();
  }
};

/// @brief TeCのスタックトレースを出力して終了する。
/// @param tec TeC
[[noreturn]] static inline void ErrorWithStackTrace(const TeC &tec) {
  std::string msg;
  msg.reserve(1024);
  msg += "INVALID INSTRUCTION.\n";
  const uint8_t pc = tec.getReg(Reg::PC);
  const uint8_t sp = tec.getReg(Reg::SP);
  // PC
  msg += std::format("PC: {:0>3X}H\n", pc);
  // [PC - 4], [PC - 3], [PC - 2], [PC - 1], [PC]
  for (uint8_t i = 0; i < 5; ++i) {
    const uint8_t addr = static_cast<uint8_t>((pc - (4 - i)) & 0xFF);
    msg += std::format("[{:0>3X}H]: {:0>3X}H\n", addr, tec.getMM(addr));
  }
  // SP
  msg += std::format("SP: {:0>3X}H\n", sp);
  // [SP - 2], [SP - 1], [SP], [SP + 1], [SP + 2]
  for (uint8_t i = 0; i < 5; ++i) {
    const uint8_t addr = static_cast<uint8_t>((sp - (4 - i)) & 0xFF);
    msg += std::format("[{:0>3X}H]: {:0>3X}H\n", addr, tec.getMM(addr));
  }
  // G0, G1, G2, SP
  msg += std::format("G0: {:0>3X}H, G1: {:0>3X}H, G2: {:0>3X}H, SP: {:0>3X}H\n",
                     tec.getReg(Reg::G0), tec.getReg(Reg::G1),
                     tec.getReg(Reg::G2), tec.getReg(Reg::SP));
  // C, S, Z
  msg += std::format("C: {}, S: {}, Z: {}", tec.getFlg(Flg::C) ? '1' : '0',
                     tec.getFlg(Flg::S) ? '1' : '0',
                     tec.getFlg(Flg::Z) ? '1' : '0');
  Error(msg, ErrorType::Program);
}

int main(int argc, char const *argv[]) {
  if (argc < 2 || 3 < argc) {
    Usage(argv[0]);
  }
  Source source = readSource(argv[1]);
  NameTable nameTable = {};
  if (argc == 3) {
    nameTable = ReadNameTable(argv[2]);
  }
  EventList events = ReadInput(nameTable);
  std::deque<uint8_t> serialInBuf{};
  TeC tec{};
  tec.writeProg(source.start, source.size, source.values);
  Printer printer;
  for (size_t i = 0; i < events.size(); ++i) {
    switch (events[i]->type) {
    case EventType::SetReg: {
      const SetRegEvent &e = static_cast<const SetRegEvent &>(*events[i]);
      tec.setReg(e.reg, e.value);
    } break;
    case EventType::SetFlg: {
      const SetFlgEvent &e = static_cast<const SetFlgEvent &>(*events[i]);
      tec.setFlg(e.flg, e.val);
    } break;
    case EventType::SetMM: {
      const SetMMEvent &e = static_cast<const SetMMEvent &>(*events[i]);
      tec.setMM(e.addr, e.val);
    } break;
    case EventType::SetDataSW: {
      const SetDataSWEvent &e = static_cast<const SetDataSWEvent &>(*events[i]);
      tec.setDataSW(e.val);
    } break;
    case EventType::SetSerialMode: {
      const SetSerialModeEvent &e =
          static_cast<const SetSerialModeEvent &>(*events[i]);
      printer.setSerialMode(e.mode);
    } break;
    case EventType::SetPrintMode: {
      const SetPrintModeEvent &e =
          static_cast<const SetPrintModeEvent &>(*events[i]);
      printer.setPrintMode(e.mode);
    } break;
    case EventType::Run:
      tec.run();
      break;
    case EventType::Stop:
      tec.stop();
      break;
    case EventType::Reset:
      tec.reset();
      break;
    case EventType::PrintReg: {
      const PrintRegEvent &e = static_cast<const PrintRegEvent &>(*events[i]);
      printer.print(tec.getReg(e.reg));
    } break;
    case EventType::PrintFlg: {
      const PrintFlgEvent &e = static_cast<const PrintFlgEvent &>(*events[i]);
      printer.print(tec.getFlg(e.flg) ? 1 : 0);
    } break;
    case EventType::PrintMM: {
      const PrintMMEvent &e = static_cast<const PrintMMEvent &>(*events[i]);
      printer.print(tec.getMM(e.addr));
    } break;
    case EventType::WaitStates: {
      const WaitStatesEvent &e =
          static_cast<const WaitStatesEvent &>(*events[i]);
      uint64_t states = 0;
      while (states < e.states && tec.isRunning()) {
        states += tec.clock(std::min(TeC::SerialUnitStates, e.states - states));
        if (const std::optional<uint8_t> serial = tec.tryReadSerialOut()) {
          printer.serial(serial.value());
        }
        if ((not serialInBuf.empty()) &&
            tec.tryWriteSerialIn(serialInBuf.front())) {
          serialInBuf.pop_front();
        }
        if (tec.isError()) {
          ErrorWithStackTrace(tec);
        }
      }
    } break;
    case EventType::WaitSerial: {
      while (tec.isRunning() &&
             (tec.isSerialInFull() || not serialInBuf.empty())) {
        tec.clock();
        if (const std::optional<uint8_t> serial = tec.tryReadSerialOut()) {
          printer.serial(serial.value());
        }
        if ((not serialInBuf.empty()) &&
            tec.tryWriteSerialIn(serialInBuf.front())) {
          serialInBuf.pop_front();
        }
        if (tec.isError()) {
          ErrorWithStackTrace(tec);
        }
      }
    } break;
    case EventType::WaitStop:
      while (tec.isRunning()) {
        tec.clock();
        if (const std::optional<uint8_t> serial = tec.tryReadSerialOut()) {
          printer.serial(serial.value());
        }
        if ((not serialInBuf.empty()) &&
            tec.tryWriteSerialIn(serialInBuf.front())) {
          serialInBuf.pop_front();
        }
        if (tec.isError()) {
          ErrorWithStackTrace(tec);
        }
      }
      break;
    case EventType::Serial: {
      const SerialEvent &e = static_cast<const SerialEvent &>(*events[i]);
      serialInBuf.insert(serialInBuf.end(), e.value.begin(), e.value.end());
    } break;
    case EventType::Write: {
      if (not tec.isRunning()) {
        Error("TeC is not running.", ErrorType::Program);
      }
      tec.write();
    } break;
    case EventType::ParallelWrite: {
      const ParallelWriteEvent &e =
          static_cast<const ParallelWriteEvent &>(*events[i]);
      tec.writeParallel(e.value);
    } break;
    case EventType::PrintParallel:
      printer.print(tec.readParallel());
      break;
    case EventType::PrintExtParallel:
      printer.print(tec.readExtParallel());
      break;
    case EventType::PrintBuz:
      printer.print(tec.getBuz() ? 1 : 0);
      break;
    case EventType::PrintSpk:
      printer.print(tec.getSpk() ? 1 : 0);
      break;
    case EventType::PrintRun:
      printer.print(tec.isRunning() ? 1 : 0);
      break;
    case EventType::Analog: {
      const AnalogEvent &e = static_cast<const AnalogEvent &>(*events[i]);
      tec.writeAnalog(e.pin, e.value);
    } break;
    }
  }
  // 出力をフラッシュ
  printer.flush();
  std::cout << std::flush;
  assert(not tec.isRunning());
  return 0;
}

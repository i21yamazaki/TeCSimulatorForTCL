#include <array>
#include <cassert>
#include <cstdint>
#include <format>
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>
#include <variant>

/// @brief 使用方法を出力して終了する。
/// @param cmd コマンド
[[noreturn]] static void Usage(const char *cmd) {
  std::cerr << std::format("usage: {} <program>.t7\n", cmd);
  std::exit(1);
}

/// @brief エラー発生フラグ
static bool HasErrorOccured = false;

/// @brief エラーメッセージを出力する。
/// @param msg エラーメッセージ
static void PrintError(const std::string &msg) {
  HasErrorOccured = true;
  std::cerr << msg << '\n';
}

/// @brief エラーが発生していれば終了する。
static void CheckError() {
  if (HasErrorOccured) {
    std::exit(1);
  }
}

/// @brief エラーメッセージを出力して終了する。
/// @param msg エラーメッセージ
[[noreturn]] static void Error(const std::string &msg) {
  PrintError(msg);
  std::exit(1);
}

// バグ発生時の出力
#define BUG(msg) Error(std::format("{}:{}: {}", __FILE__, __LINE__, msg))

/// @brief ラベル
static std::unordered_map<std::string, uint8_t> labels = {};

/// @brief 入力
static std::ifstream ifs;

/// @brief 現在読んでいる行番号
static size_t curLineNum;

/// @brief 現在読んでいる行
static std::string curLine;

/// @brief 現在の文字の添え字
static size_t curIdx;

/// @brief 空白文字とコメントを読み飛ばす。
static inline void SkipSpaceOrComment() {
  while (curIdx < curLine.size()) {
    if (curLine[curIdx] == ';') {
      curIdx = curLine.size();
      break;
    } else if (std::isspace(curLine[curIdx])) {
      ++curIdx;
    } else {
      break;
    }
  }
}

/// @brief 1文字判定する。
/// @param ch 判定する文字
/// @return 等しい場合は true, そうでなければ false
static inline bool IsCh(const char ch) noexcept {
  if (curIdx < curLine.size() && curLine[curIdx] == ch) {
    ++curIdx;
    return true;
  }
  return false;
}

/// @brief 現在の文字が名前の開始文字であるか判定する。
/// @return 名前の開始文字なら true, そうでなければ false
static inline bool IsNameStart() {
  return curIdx < curLine.size() &&
         (std::isalpha(curLine[curIdx]) || curLine[curIdx] == '_');
}

/// @brief 現在の文字が名前文字であるか判定する。
/// @return 名前文字なら true, そうでなければ false
static inline bool IsName() {
  return curIdx < curLine.size() &&
         (std::isalnum(curLine[curIdx]) || curLine[curIdx] == '_');
}

/// @brief 名前を取得する。
/// @return 名前
/// @note 事前条件: IsNameStart() == true
static inline std::string GetName() {
  assert(IsNameStart());
  std::string name{};
  do {
    name += static_cast<char>(std::toupper(curLine[curIdx++]));
  } while (IsName());
  return name;
}

/// @brief 名前を解析して読み飛ばす。
static inline void ParseName() {
  assert(IsNameStart());
  do {
    ++curIdx;
  } while (IsName());
}

/// @brief 文法エラーのエラーメッセージを出力する。
/// @param msg エラーメッセージ
static inline void PrintSyntaxError(const std::string &msg) {
  PrintError(std::format("{:>3}: {}\n{}", curLineNum, msg, curLine));
}

/// @brief 現在の文字が10進数文字であるか判定する。
/// @return 10進数文字なら true, そうでなければ false
[[nodiscard]] static inline bool IsDigit() {
  return curIdx < curLine.size() && std::isdigit(curLine[curIdx]);
}

/// @brief 現在の文字が16進数文字であるか判定する。
/// @return 16進数文字なら true, そうでなければ false
[[nodiscard]] static inline bool IsXDigit() {
  return curIdx < curLine.size() && std::isxdigit(curLine[curIdx]);
}

/// @brief 数値を解析して読み飛ばす。
/// @return 解析が成功すれば true, そうでなければ false
[[nodiscard]] static inline bool ParseNum() {
  assert(IsDigit());
  bool isHex = false;
  do {
    if (not IsDigit()) {
      isHex = true;
    }
    ++curIdx;
  } while (IsXDigit());
  if (IsCh('H') || IsCh('h')) {
    isHex = true;
  } else if (isHex) {
    PrintSyntaxError("'H' expected.");
    return false;
  }
  return true;
}

// 再帰呼び出しのため
static bool ParseAdd();

/// @brief 値を解析して読み飛ばす。
/// @return 解析が成功すれば true, そうでなければ false
[[nodiscard]] static bool ParseVal() {
  SkipSpaceOrComment();
  if (IsCh('+') || IsCh('-')) {
    SkipSpaceOrComment();
  }

  if (IsCh('(')) {
    if (not ParseAdd()) {
      return false;
    }
    if (not IsCh(')')) {
      PrintSyntaxError("')' expected.");
      return false;
    }
  } else if (IsCh('\'')) {
    if (curLine.size() <= curIdx || not std::isprint(curLine[curIdx])) {
      PrintSyntaxError("invalid character.");
      return false;
    }
    ++curIdx; // 文字
    if (not IsCh('\'')) {
      PrintSyntaxError("' expected.");
      return false;
    }
  } else if (IsDigit()) {
    if (not ParseNum()) {
      return false;
    }
  } else if (IsNameStart()) {
    ParseName();
  } else {
    PrintSyntaxError("expression expected.");
    return false;
  }
  return true;
}

/// @brief 乗除算を解析して読み飛ばす。
/// @return 解析が成功すれば true, そうでなければ false
[[nodiscard]] static bool ParseMul() {
  if (not ParseVal()) {
    return false;
  }
  for (;;) {
    SkipSpaceOrComment();
    if (IsCh('*') || IsCh('/')) {
      if (not ParseVal()) {
        return false;
      }
    } else {
      break;
    }
  }
  return true;
}

/// @brief 加減算を解析して読み飛ばす。
/// @return 解析が成功すれば true, そうでなければ false
[[nodiscard]] static bool ParseAdd() {
  if (not ParseMul()) {
    return false;
  }
  for (;;) {
    SkipSpaceOrComment();
    if (IsCh('+') || IsCh('-')) {
      return false;
    } else {
      break;
    }
  }
  return true;
}

/// @brief 式を解析して読み飛ばす。
/// @param count 読んだ式のバイト数カウンタ
/// @return 解析に成功すれば true, そうでなければ false
[[nodiscard]] static bool ParseExpr(uint8_t &count) {
  SkipSpaceOrComment();
  if (IsCh('"')) {
    while (curIdx < curLine.size() && std::isprint(curLine[curIdx]) &&
           curLine[curIdx] != '"') {
      ++count;
      ++curIdx;
    }
    if (not IsCh('"')) {
      PrintSyntaxError("\" expected.");
      return false;
    }
  } else {
    if (not ParseAdd()) {
      return false;
    }
    ++count;
  }
  return true;
}

/// @brief 式リストを解析して読み飛ばす。
/// @param count 読んだ式の合計バイト数カウンタ
/// @return 解析に成功すれば true, そうでなければ false
[[nodiscard]] static bool ParseExprList(uint8_t &count) {
  if (not ParseExpr(count)) {
    return false;
  }
  for (;;) {
    SkipSpaceOrComment();
    if (IsCh(',')) {
      if (not ParseExpr(count)) {
        return false;
      }
    } else {
      break;
    }
  }
  return count;
}

/// @brief 数値を読み取る。
/// @param val 読み取った値
/// @return 読み取りが成功すれば true, そうでなければ false
[[nodiscard]] static bool GetNum(uint8_t &val) {
  assert(curIdx < curLine.size() && std::isdigit(curLine[curIdx]));
  bool isHex = false;
  std::string numStr;
  do {
    if (not std::isdigit(curLine[curIdx])) {
      isHex = true;
    }
    numStr += curLine[curIdx++];
  } while (curIdx < curLine.size() && std::isxdigit(curLine[curIdx]));
  if (IsCh('H') || IsCh('h')) {
    isHex = true;
  } else if (isHex) {
    PrintSyntaxError("'H' expected.");
    return false;
  }
  try {
    size_t lastIdx;
    val = static_cast<uint8_t>(std::stoi(numStr, &lastIdx, isHex ? 16 : 10));
    if (lastIdx != numStr.size()) {
      BUG("stoi");
    }
  } catch (const std::invalid_argument &e) {
    BUG("stoi");
  } catch (const std::out_of_range &e) {
    PrintSyntaxError(std::format("too big number. (value: {})", numStr));
    return false;
  }
  return true;
}

// 再帰呼び出しのため
static bool GetAdd(uint8_t &val);

/// @brief 値を読みとる。
/// @param val 読み取った値
/// @return 解析が成功すれば true, そうでなければ false
[[nodiscard]] static bool GetVal(uint8_t &val) {
  SkipSpaceOrComment();
  bool pos = true;
  if (IsCh('+')) {
    SkipSpaceOrComment();
  } else if (IsCh('-')) {
    SkipSpaceOrComment();
    pos = false;
  }

  if (IsCh('(')) {
    if (not GetAdd(val)) {
      return false;
    }
    if (not IsCh(')')) {
      PrintSyntaxError("')' expected.");
      return false;
    }
  } else if (IsCh('\'')) {
    if (curLine.size() <= curIdx || not std::isprint(curLine[curIdx])) {
      PrintSyntaxError("invalid character.");
      return false;
    }
    val = curLine[curIdx++];
    if (not IsCh('\'')) {
      PrintSyntaxError("' expected.");
      return false;
    }
  } else if (IsDigit()) {
    if (not GetNum(val)) {
      return false;
    }
  } else if (IsNameStart()) {
    std::string label = GetName();
    if (const auto labelIt = labels.find(label); labelIt != labels.end()) {
      val = labelIt->second;
    } else {
      PrintSyntaxError(std::format("label: \"{}\" not found.", label));
      return false;
    }
  } else {
    PrintSyntaxError("expression expected.");
    return false;
  }
  if (not pos) {
    val = static_cast<uint8_t>((~val) + 1);
  }
  return true;
}

/// @brief 乗除算を読み取る。
/// @param val 読み取った値
/// @return 解析が成功すれば true, そうでなければ false
[[nodiscard]] static bool getMul(uint8_t &val) {
  if (not GetVal(val)) {
    return false;
  }
  for (;;) {
    SkipSpaceOrComment();
    if (IsCh('*')) {
      uint8_t rVal;
      if (not GetVal(rVal)) {
        return false;
      }
      val *= rVal;
    } else if (IsCh('/')) {
      uint8_t rVal;
      if (not GetVal(rVal)) {
        return false;
      }
      if (rVal == 0x00) {
        PrintSyntaxError("zero division detected.");
        return false;
      }
      val /= rVal;
    } else {
      break;
    }
  }
  return true;
}

/// @brief 加減算を読み取る。
/// @param val 読み取った値
/// @return 解析が成功すれば true, そうでなければ false
[[nodiscard]] static bool GetAdd(uint8_t &val) {
  if (not getMul(val)) {
    return false;
  }
  for (;;) {
    SkipSpaceOrComment();
    if (IsCh('+')) {
      uint8_t rVal;
      if (not getMul(rVal)) {
        return false;
      }
      val += rVal;
    } else if (IsCh('-')) {
      uint8_t rVal;
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

/// @brief 式を読み取る。
/// @param binary 読み取った値を書き込むための生成中のバイナリ
/// @param curAddr 書き込むアドレス
/// @return 解析が成功すれば true, そうでなければ false
[[nodiscard]] static bool getExpr(std::array<uint8_t, 256> &binary,
                                  uint8_t &curAddr) {
  SkipSpaceOrComment();
  if (IsCh('"')) {
    while (curIdx < curLine.size() && std::isprint(curLine[curIdx]) &&
           curLine[curIdx] != '"') {
      binary[curAddr++] = curLine[curIdx++];
    }
    if (not IsCh('"')) {
      PrintSyntaxError("\" expected.");
      return false;
    }
  } else {
    if (not GetAdd(binary[curAddr++])) {
      return false;
    }
  }
  return true;
}

/// @brief 式リストを読み取る。
/// @param binary 読み取った値を書き込むための生成中のバイナリ
/// @param curAddr 書き込むアドレス
/// @return 解析が成功すれば true, そうでなければ false
[[nodiscard]] static bool getExprList(std::array<uint8_t, 256> &binary,
                                      uint8_t &curAddr) {
  if (not getExpr(binary, curAddr)) {
    return false;
  }
  for (;;) {
    SkipSpaceOrComment();
    if (IsCh(',')) {
      if (not getExpr(binary, curAddr)) {
        return false;
      }
    } else {
      break;
    }
  }
  return true;
}

/// @brief タイプ1の命令 (NO|EI|DI|RET|RETI|HALT)
class InstType1 {
public:
  constexpr InstType1(const uint8_t bin) noexcept : m_bin(bin) {}

  void getBin(std::array<uint8_t, 256> &bin, uint8_t &curIdx) const noexcept {
    bin[curIdx++] = m_bin;
  }

  constexpr uint8_t getSize() const noexcept { return 1; }

private:
  uint8_t m_bin;
};

/// @brief NO命令
static constexpr InstType1 NO{0x00};
/// @brief EI命令
static constexpr InstType1 EI{0xE0};
/// @brief DI命令
static constexpr InstType1 DI{0xE3};
/// @brief RET命令
static constexpr InstType1 RET{0xEC};
/// @brief RETI命令
static constexpr InstType1 RETI{0xEF};
/// @brief HALT命令
static constexpr InstType1 HALT{0xFF};
/// @brief 汎用レジスタ
enum class GR : uint8_t { G0 = 0x00, G1 = 0x04, G2 = 0x08, SP = 0x0C };
/// @brief タイプ2の命令 (SHLA|SHLL|SHRA|SHRL|PUSH|POP)
class InstType2 {
public:
  constexpr InstType2(const uint8_t bin) noexcept : m_bin(bin) {}

  void getBin(std::array<uint8_t, 256> &bin, uint8_t &curIdx,
              const GR gr) const noexcept {
    bin[curIdx++] = static_cast<uint8_t>(m_bin | static_cast<uint8_t>(gr));
  }

  constexpr uint8_t getSize() const noexcept { return 1; }

private:
  uint8_t m_bin;
};
/// @brief SHLA命令
static constexpr InstType2 SHLA{0x90};
/// @brief SHLL命令
static constexpr InstType2 SHLL{0x91};
/// @brief SHRA命令
static constexpr InstType2 SHRA{0x92};
/// @brief SHRL命令
static constexpr InstType2 SHRL{0x93};
/// @brief PUSH命令
static constexpr InstType2 PUSH{0xD0};
/// @brief POP命令
static constexpr InstType2 POP{0xD2};
/// @brief タイプ3の命令(IN|OUT)
class InstType3 {
public:
  constexpr InstType3(const uint8_t bin) noexcept : m_bin(bin) {}

  void getBin(std::array<uint8_t, 256> &bin, uint8_t &curIdx, const GR gr,
              const uint8_t addr) const noexcept {
    bin[curIdx++] = static_cast<uint8_t>(m_bin | static_cast<uint8_t>(gr));
    bin[curIdx++] = addr;
  }

  constexpr uint8_t getSize() const noexcept { return 2; }

private:
  uint8_t m_bin;
};

/// @brief IN命令
static constexpr InstType3 IN{0xC0};
/// @brief OUT命令
static constexpr InstType3 OUT{0xC3};

/// @brief アドレッシングモード
enum class XR : uint8_t {
  Direct = 0x00,
  G1Idx = 0x01,
  G2Idx = 0x02,
  Imm = 0x03
};

/// @brief タイプ4の命令 (LD|ADD|SUB|CMP|AND|OR|XOR)
class InstType4 {
public:
  constexpr InstType4(const uint8_t bin) noexcept : m_bin(bin) {}

  void getBin(std::array<uint8_t, 256> &bin, uint8_t &curIdx, const GR gr,
              const XR xr, const uint8_t addr) const noexcept {
    bin[curIdx++] = static_cast<uint8_t>(m_bin | static_cast<uint8_t>(gr) |
                                         static_cast<uint8_t>(xr));
    bin[curIdx++] = addr;
  }

  constexpr size_t getSize() const noexcept { return 2; }

private:
  uint8_t m_bin;
};

/// @brief LD命令
static constexpr InstType4 LD{0x10};
/// @brief ADD命令
static constexpr InstType4 ADD{0x30};
/// @brief SUB命令
static constexpr InstType4 SUB{0x40};
/// @brief CMP命令
static constexpr InstType4 CMP{0x50};
/// @brief AND命令
static constexpr InstType4 AND{0x60};
/// @brief OR命令
static constexpr InstType4 OR{0x70};
/// @brief XOR命令
static constexpr InstType4 XOR{0x80};

/// @brief タイプ5の命令 (ST)
class InstType5 {
public:
  constexpr InstType5(const uint8_t bin) noexcept : m_bin(bin) {}

  void getBin(std::array<uint8_t, 256> &bin, uint8_t &curIdx, const GR gr,
              const XR xr, const uint8_t addr) const noexcept {
    bin[curIdx++] = static_cast<uint8_t>(m_bin | static_cast<uint8_t>(gr) |
                                         static_cast<uint8_t>(xr));
    bin[curIdx++] = addr;
  }

  constexpr size_t getSize() const noexcept { return 2; }

private:
  uint8_t m_bin;
};

/// @brief ST命令
static constexpr InstType5 ST{0x20};

/// @brief タイプ6の命令 (JMP|JZ|JC|JM|CALL|JNZ|JNC|JNM)
class InstType6 {
public:
  constexpr InstType6(const uint8_t bin) noexcept : m_bin(bin) {}

  void getBin(std::array<uint8_t, 256> &bin, uint8_t &curIdx, const XR xr,
              const uint8_t addr) const noexcept {
    bin[curIdx++] = static_cast<uint8_t>(m_bin | static_cast<uint8_t>(xr));
    bin[curIdx++] = addr;
  }

  constexpr size_t getSize() const noexcept { return 2; }

private:
  uint8_t m_bin;
};

/// @brief JMP命令
static constexpr InstType6 JMP{0xA0};
/// @brief JZ命令
static constexpr InstType6 JZ{0xA4};
/// @brief JC命令
static constexpr InstType6 JC{0xA8};
/// @brief JM命令
static constexpr InstType6 JM{0xAC};
/// @brief CALL命令
static constexpr InstType6 CALL{0xB0};
/// @brief JNZ命令
static constexpr InstType6 JNZ{0xB4};
/// @brief JNC命令
static constexpr InstType6 JNC{0xB8};
/// @brief JNM命令
static constexpr InstType6 JNM{0xBC};

/// @brief 命令
using InstType = std::variant<InstType1, InstType2, InstType3, InstType4,
                              InstType5, InstType6>;

/// @brief ニーモニックと命令の対応表
static const std::unordered_map<std::string, InstType> InstList = {
    // Type1
    {"NO", NO},
    {"EI", EI},
    {"DI", DI},
    {"RET", RET},
    {"RETI", RETI},
    {"HALT", HALT},
    // Type2
    {"SHLA", SHLA},
    {"SHLL", SHLL},
    {"SHRA", SHRA},
    {"SHRL", SHRL},
    {"PUSH", PUSH},
    {"POP", POP},
    // Type3
    {"IN", IN},
    {"OUT", OUT},
    // Type4
    {"LD", LD},
    {"ADD", ADD},
    {"SUB", SUB},
    {"CMP", CMP},
    {"AND", AND},
    {"OR", OR},
    {"XOR", XOR},
    // Type5
    {"ST", ST},
    // Type6
    {"JMP", JMP},
    {"JZ", JZ},
    {"JC", JC},
    {"JM", JM},
    {"CALL", CALL},
    {"JNZ", JNZ},
    {"JNC", JNC},
    {"JNM", JNM}};

static inline void Pass1Line(uint8_t &curAddr) {
  // ラベル
  std::string label{};
  if (IsNameStart()) {
    label = GetName();
  }
  // ラベルの値
  uint8_t labelNum = curAddr;
  SkipSpaceOrComment();
  if (IsNameStart()) {
    const std::string inst = GetName();
    if (inst == "EQU") {
      uint8_t val = 0x00;
      if (not GetAdd(val)) {
        return;
      }
      labelNum = val;
    } else if (inst == "ORG") {
      uint8_t val = 0x00;
      if (not GetAdd(val)) {
        return;
      }
      labelNum = val;
      curAddr = val;
    } else if (inst == "DS") {
      uint8_t val = 0x00;
      if (not GetAdd(val)) {
        return;
      }
      curAddr += val;
    } else if (inst == "DC") {
      uint8_t count = 0;
      if (not ParseExprList(count)) {
        return;
      }
      curAddr += count;
    } else if (const auto instIt = InstList.find(inst);
               instIt != InstList.cend()) {
      curAddr += std::visit(
          [](const auto &inst) noexcept -> uint8_t { return inst.getSize(); },
          instIt->second);
      // 以降を全て読み飛ばす
      curIdx = curLine.size();
    } else {
      PrintSyntaxError(std::format("unkown instruction. (inst: \"{}\")", inst));
      return;
    }
  }
  // ラベルがあればアドレスを登録
  if (not label.empty()) {
    labels.emplace(label, labelNum);
  }
}

/// @brief パス1（ラベルの割り当てなど）を実行する。
static inline void Pass1() {
  uint8_t curAddr = 0x00;
  curLineNum = 0;
  while (std::getline(ifs, curLine)) {
    curIdx = 0;
    // 行番号を進める（最初の行が1）
    ++curLineNum;
    Pass1Line(curAddr);
  }
  // エラー発生ならここで中止
  CheckError();
}

/// @brief 機械語列用の配列型
using BinaryT = std::array<uint8_t, 256>;

/// @brief Pass2の1行分の処理を行う。
/// @param start 開始アドレス
/// @param curAddr 現在のアドレス
/// @param binary 機械語列
static inline void Pass2Line(uint8_t &start, uint8_t &curAddr,
                             BinaryT &binary) {
  // 名前から始まっている場合
  if (IsNameStart()) {
    // ラベルを読み飛ばす
    ParseName();
  }
  // 空白を読み飛ばす
  SkipSpaceOrComment();
  // 名前があれば命令
  if (IsNameStart()) {
    // 命令
    const std::string inst = GetName();
    if (inst == "EQU") {
      // EQU命令は読み飛ばす（パス1で解析済みのため）
      if (not ParseAdd()) {
        return;
      }
    } else if (inst == "ORG") {
      // ORG命令のオペランドを解析
      uint8_t val = 0;
      if (not GetAdd(val)) {
        return;
      }
      if (curAddr == 0x00) {
        // ORG命令より前に機械語命令がなければ
        // 開始アドレスを変更
        start = val;
        curAddr = val;
      } else {
        // すでに機械語命令が生成されている場合は、
        // 00H で埋める
        while (curAddr < val) {
          binary[curAddr++] = 0x00;
        }
      }
    } else if (inst == "DS") {
      // DS命令は解析済みだが
      // 記録していないのでもう一度解析
      uint8_t val = 0;
      if (not GetAdd(val)) {
        return;
      }
      // 00H で埋める
      while (0 < val--) {
        binary[curAddr++] = 0x00;
      }
    } else if (inst == "DC") {
      // DC命令
      if (not getExprList(binary, curAddr)) {
        return;
      }
    } else if (const auto instIt = InstList.find(inst);
               instIt != InstList.cend()) {
      // 機械語命令
      // レジスタリスト
      static const std::unordered_map<std::string, GR> grList = {
          {"G0", GR::G0}, {"G1", GR::G1}, {"G2", GR::G2}, {"SP", GR::SP}};
      // 命令のタイプごとで分岐
      if (std::holds_alternative<InstType1>(instIt->second)) {
        // タイプ1の命令（オペランドなし）
        const InstType1 &inst1 = std::get<InstType1>(instIt->second);
        inst1.getBin(binary, curAddr);
      } else if (std::holds_alternative<InstType2>(instIt->second)) {
        // タイプ2の命令
        const InstType2 &inst2 = std::get<InstType2>(instIt->second);
        // 空白を読み飛ばす
        SkipSpaceOrComment();
        // レジスタ名が必要
        if (not IsNameStart()) {
          PrintSyntaxError("register expected.");
          return;
        }
        // レジスタ名
        const std::string reg = GetName();
        if (const auto grIt = grList.find(reg); grIt != grList.cend()) {
          inst2.getBin(binary, curAddr, grIt->second);
        } else {
          PrintSyntaxError(std::format("invalid register. (value: {})", reg));
          return;
        }
      } else if (std::holds_alternative<InstType3>(instIt->second)) {
        // タイプ3の命令 (IN or OUT)
        const InstType3 &inst3 = std::get<InstType3>(instIt->second);
        // 空白を読み飛ばす
        SkipSpaceOrComment();
        // レジスタ名が必要
        if (not IsNameStart()) {
          PrintSyntaxError("register expected.");
          return;
        }
        // レジスタ名
        const std::string reg = GetName();
        // レジスタ
        GR gr = GR::G0;
        if (const auto grIt = grList.find(reg); grIt != grList.cend()) {
          gr = grIt->second;
        } else {
          PrintSyntaxError(std::format("invalid register. (value: {})", reg));
          return;
        }
        // 空白を読み飛ばす
        SkipSpaceOrComment();
        // ',' が必要
        if (not IsCh(',')) {
          PrintSyntaxError("',' expected.");
          return;
        }
        // アドレス
        uint8_t addr = 0x00;
        if (not GetAdd(addr)) {
          return;
        }
        // IOアドレスが 10H 以上ならエラー
        if (0x10 <= addr) {
          PrintSyntaxError(std::format(
              "io address must be lower than 10H. (address: 0{:>2X}H)",
              static_cast<unsigned int>(addr)));
          return;
        }
        inst3.getBin(binary, curAddr, gr, addr);
      } else if (std::holds_alternative<InstType4>(instIt->second)) {
        // タイプ4の命令
        const InstType4 &inst4 = std::get<InstType4>(instIt->second);
        SkipSpaceOrComment();
        if (not IsNameStart()) {
          PrintSyntaxError("register expected.");
          return;
        }
        // レジスタ名
        const std::string reg = GetName();
        // レジスタ
        GR gr = GR::G0;
        if (const auto grIt = grList.find(reg); grIt != grList.cend()) {
          gr = grIt->second;
        } else {
          PrintSyntaxError(std::format("invalid register. (value: {})", reg));
          return;
        }
        // 空白を読み飛ばす
        SkipSpaceOrComment();
        // ',' が必要
        if (not IsCh(',')) {
          PrintSyntaxError("',' expected.");
          return;
        }
        // 空白を読み飛ばす
        SkipSpaceOrComment();
        // アドレッシングモード
        XR xr = XR::Direct;
        // アドレス
        uint8_t addr = 0x00;
        if (IsCh('#')) {
          // '#' があれば即値
          xr = XR::Imm;
          if (not GetAdd(addr)) {
            return;
          }
        } else {
          if (not GetAdd(addr)) {
            return;
          }
          // 空白を読み飛ばす
          SkipSpaceOrComment();
          if (IsCh(',')) {
            // ',' があればインデックスモード
            // 空白を読み飛ばす
            SkipSpaceOrComment();
            if (not IsNameStart()) {
              PrintSyntaxError("index register expected.");
              return;
            }
            // インデックスレジスタ名
            const std::string idxReg = GetName();
            if (idxReg == "G1") {
              xr = XR::G1Idx;
            } else if (idxReg == "G2") {
              xr = XR::G2Idx;
            } else {
              PrintSyntaxError(
                  std::format("index register expected. (value: {})", idxReg));
              return;
            }
          }
        }
        inst4.getBin(binary, curAddr, gr, xr, addr);
      } else if (std::holds_alternative<InstType5>(instIt->second)) {
        // タイプ5の命令 (ST)
        const InstType5 &inst5 = std::get<InstType5>(instIt->second);
        SkipSpaceOrComment();
        if (not IsNameStart()) {
          PrintSyntaxError("register expected.");
          return;
        }
        // レジスタ名
        const std::string reg = GetName();
        // レジスタ
        GR gr = GR::G0;
        if (const auto grIt = grList.find(reg); grIt != grList.cend()) {
          gr = grIt->second;
        } else {
          PrintSyntaxError("register expected.");
          return;
        }
        // 空白を読み飛ばす
        SkipSpaceOrComment();
        // ',' が必要
        if (not IsCh(',')) {
          PrintSyntaxError("',' expected.");
          return;
        }
        // 空白を読み飛ばす
        SkipSpaceOrComment();
        // タイプ5の命令で即値は使用不可
        if (IsCh('#')) {
          PrintSyntaxError("immediate addressing mode is not allowed in ST "
                           "instruction.");
          return;
        }
        // アドレス
        uint8_t addr = 0x00;
        // アドレッシングモード
        XR xr = XR::Direct;
        if (not GetAdd(addr)) {
          return;
        }
        // 空白を読み飛ばす
        SkipSpaceOrComment();
        if (IsCh(',')) {
          // ',' があればインデックスモード
          SkipSpaceOrComment();
          if (not IsNameStart()) {
            PrintSyntaxError("index register expected.");
            return;
          }
          // インデックスレジスタ名
          const std::string idxReg = GetName();
          if (idxReg == "G1") {
            xr = XR::G1Idx;
          } else if (idxReg == "G2") {
            xr = XR::G2Idx;
          } else {
            PrintSyntaxError(
                std::format("index register expected. (value: {})", idxReg));
            return;
          }
        }
        inst5.getBin(binary, curAddr, gr, xr, addr);
      } else if (std::holds_alternative<InstType6>(instIt->second)) {
        // タイプ6の命令
        const InstType6 &inst6 = std::get<InstType6>(instIt->second);
        uint8_t addr = 0x00;
        if (not GetAdd(addr)) {
          return;
        }
        XR xr = XR::Direct;
        // 空白を読み飛ばす
        SkipSpaceOrComment();
        if (IsCh(',')) {
          // ',' があればインデックスモード
          // 空白を読み飛ばす
          SkipSpaceOrComment();
          if (not IsNameStart()) {
            PrintSyntaxError("index register expected.");
            return;
          }
          // インデックスレジスタ名
          const std::string idxReg = GetName();
          if (idxReg == "G1") {
            xr = XR::G1Idx;
          } else if (idxReg == "G2") {
            xr = XR::G2Idx;
          } else {
            PrintSyntaxError(
                std::format("index register expected. (value: {})", idxReg));
            return;
          }
        }
        inst6.getBin(binary, curAddr, xr, addr);
      } else {
        BUG("unkown instruction.");
      }
    } else {
      PrintSyntaxError(std::format("unkown instruction. (inst: \"{}\")", inst));
      return;
    }
  }
  // 空白を読み飛ばす
  SkipSpaceOrComment();
  // 不正なオペランドが残っていたらエラー
  if (curIdx < curLine.size()) {
    PrintSyntaxError("invalid operand.");
  }
}

/// @brief アセンブリソースファイルの拡張子
static constexpr std::string_view ExtSrc = "t7";
/// @brief 機械語ファイルの拡張子
static constexpr std::string_view ExtBinary = "bin";
/// @brief 名前表ファイルの拡張子
static constexpr std::string_view ExtNameTable = "nt";

/// @brief パス2（コード生成など）を実行する。
/// @param progname アセンブル中のプログラム名
static inline void Pass2(const std::string &progname) {
  // 開始アドレス
  uint8_t start = 0x00;
  // 現在のアドレス
  uint8_t curAddr = 0x00;
  // 機械語列
  BinaryT binary{};
  // 行番号を初期化
  curLineNum = 0;
  // 1行づつ処理
  while (std::getline(ifs, curLine)) {
    // 文字の添え字を初期化
    curIdx = 0;
    // 行番号を進める
    ++curLineNum;
    // 1行分の処理
    Pass2Line(start, curAddr, binary);
  }
  // エラーチェック
  CheckError();
  // プログラムサイズ
  const uint8_t size = static_cast<uint8_t>(curAddr - start);
  {
    // 機械語ファイルの名前
    const std::string binaryFileName =
        std::format("{}.{}", progname, ExtBinary);
    // 機械語ファイルを開く
    std::ofstream ofs{binaryFileName,
                      std::ios_base::out | std::ios_base::binary};
    // ファイルが開かなければエラー
    if (not ofs) {
      Error(std::format("couldn't open file. (path: \"{}\")", binaryFileName));
    }
    // 開始アドレスを書き込む
    ofs.write(reinterpret_cast<const char *>(&start), 1);
    // プログラムサイズを書き込む
    ofs.write(reinterpret_cast<const char *>(&size), 1);
    // 機械語列を書き込む
    ofs.write(reinterpret_cast<const char *>(binary.data() + start), size);
  }
  {
    // 名前表ファイルの名前
    const std::string nameTableFileName =
        std::format("{}.{}", progname, ExtNameTable);
    // 名前表ファイルを開く
    std::ofstream ofs{nameTableFileName, std::ios_base::out};
    // ファイルが開かなければエラー
    if (not ofs) {
      Error(
          std::format("couldn't open file. (path: \"{}\")", nameTableFileName));
    }
    // 名前表を出力
    for (const auto &[label, addr] : labels) {
      ofs << std::format("{:<8} 0{:0>2X}H\n", label + ':',
                         static_cast<unsigned int>(addr));
    }
  }
}

int main(int argc, char const *argv[]) {
  if (argc != 2) {
    Usage(argv[0]);
  }
  // プログラム名
  std::string progname = argv[1];
  // 拡張子の確認
  if (progname.ends_with(std::format(".{}", ExtSrc))) {
    // プログラム名から拡張子を除去
    progname.erase(progname.end() - 3, progname.end());
  } else {
    Error(".t7 file expected.");
  }
  // ファイルを開く
  ifs.open(argv[1]);
  // 開けない時はエラー
  if (not ifs) {
    Error(std::format("couldn't open file. (path: \"{}\")", argv[1]));
  }
  // パス1を実行（アドレス解決）
  Pass1();
  // EOFフラグをクリア
  ifs.clear();
  // ファイルの先頭までシーク
  ifs.seekg(0, std::ios_base::beg);
  // パス2を実行（機械語と名前表生成）
  Pass2(progname);
  return 0;
}
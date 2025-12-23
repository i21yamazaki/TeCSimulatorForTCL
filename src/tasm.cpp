#include <array>
#include <cassert>
#include <cstdint>
#include <format>
#include <fstream>
#include <iostream>
#include <limits>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

/// @brief 使用方法を出力して終了する。
/// @param cmd コマンド
[[noreturn]] static void Usage(const char *cmd) {
  std::cerr << std::format("使用方法: {} <program>.t7\n", cmd);
  std::exit(1);
}

/// @brief エラー発生フラグ
static bool HasErrorOccurred = false;

/// @brief エラー or 警告 発生フラグ
static bool HasErrorOrWarningOccurred = false;

/// @brief エラーメッセージを出力する。
/// @param msg エラーメッセージ
static inline void PrintError(const std::string &msg) {
  // 2つ目以降のエラーは読みやすいように改行を挟む
  if (HasErrorOrWarningOccurred) {
    std::cerr << '\n';
  }
  HasErrorOccurred = true;
  HasErrorOrWarningOccurred = true;
  std::cerr << msg << '\n';
}

/// @brief 警告メッセージを出力する。
/// @param msg 警告メッセージ
static inline void PrintWarning(const std::string &msg) {
  // 2つ目以降の警告は読みやすいように改行を挟む
  if (HasErrorOrWarningOccurred) {
    std::cerr << '\n';
  }
  HasErrorOrWarningOccurred = true;
  std::cerr << msg << '\n';
}

/// @brief エラーが発生していれば終了する。
static inline void CheckError() {
  if (HasErrorOccurred) {
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
static std::unordered_map<std::string, std::pair<uint8_t, size_t>> Labels = {};

/// @brief 現在読んでいる行番号
static size_t CurLineNum = 0;

/// @brief 現在読んでいる行
static std::string CurLine{};

/// @brief 全ての行
static std::vector<std::string> Lines{};

/// @brief 現在の文字の添え字
static size_t CurIdx = 0;

/// @brief 行末・空白・コメントのいずれかであるか判定する。
[[nodiscard]] static inline bool IsSpaceOrComment() {
  return CurLine.size() <= CurIdx || CurLine[CurIdx] == ';' ||
         std::isspace(CurLine[CurIdx]);
}

/// @brief 空白を読み飛ばす。
static inline void SkipSpace() {
  while (CurIdx < CurLine.size() && std::isspace(CurLine[CurIdx])) {
    ++CurIdx;
  }
}

/// @brief 空白文字とコメントを読み飛ばす。
static inline void SkipSpaceOrComment() {
  while (CurIdx < CurLine.size()) {
    if (CurLine[CurIdx] == ';') {
      CurIdx = CurLine.size();
      break;
    } else if (std::isspace(CurLine[CurIdx])) {
      ++CurIdx;
    } else {
      break;
    }
  }
}

/// @brief 1文字判定する。
/// @param ch 判定する文字
/// @return 等しい場合は true, そうでなければ false
static inline bool IsCh(const char ch) noexcept {
  if (CurIdx < CurLine.size() && CurLine[CurIdx] == ch) {
    ++CurIdx;
    return true;
  }
  return false;
}

/// @brief 現在の文字が名前の開始文字であるか判定する。
/// @return 名前の開始文字なら true, そうでなければ false
static inline bool IsNameStart() {
  return CurIdx < CurLine.size() &&
         (std::isalpha(CurLine[CurIdx]) || CurLine[CurIdx] == '_');
}

/// @brief 現在の文字が名前文字であるか判定する。
/// @return 名前文字なら true, そうでなければ false
static inline bool IsName() {
  return CurIdx < CurLine.size() &&
         (std::isalnum(CurLine[CurIdx]) || CurLine[CurIdx] == '_');
}

/// @brief 名前を取得する。
/// @return 名前
/// @note 事前条件: IsNameStart() == true
static inline std::string GetName() {
  assert(IsNameStart());
  std::string name{};
  do {
    name += static_cast<char>(std::toupper(CurLine[CurIdx++]));
  } while (IsName());
  return name;
}

/// @brief 名前を解析して読み飛ばす。
static inline void ParseName() {
  assert(IsNameStart());
  do {
    ++CurIdx;
  } while (IsName());
}

/// @brief エラーコード
enum class ErrorCode : uint8_t {
  /// @brief BUG
  Bug,
  /// @brief 'H' expected.
  HExpected,
  /// @brief ')' expected.
  RPExpected,
  /// @brief register expected.
  RegisterExpected,
  /// @brief invalid character literal.
  InvalidCharLit,
  /// @brief '\'' expected.
  SingleQuotationExpected,
  /// @brief '\"' expected.
  DoubleQuotationExpected,
  /// @brief expression expected.
  ExpressionExpected,
  /// @brief undefined label
  UndefinedLabel,
  /// @brief zero division detected.
  ZeroDivision,
  /// @brief unknown instruction.
  UnknownInstruction,
  /// @brief invalid register.
  InvalidRegister,
  /// @brief ',' expected.
  CommaExpected,
  /// @brief index register expected.
  IndexRegisterExpected,
  /// @brief invalid index register.
  InvalidIndexRegister,
  /// @brief invalid immediate address.
  InvalidImmediate,
  /// @brief invalid operand.
  InvalidOperand,
  /// @brief invalid label.
  InvalidLabel,
  /// @brief duplicated label.
  DuplicatedLabel,
  /// @brief invalid org.
  InvalidOrg
};

/// @brief 警告コード
enum class WarningCode : uint8_t {
  /// @brief address out of range.
  AddressOutOfRange,
  /// @brief value out of range.
  ValueOutOfRange,
  /// @brief io address out of range.
  IOAddressOutOfRange,
  /// @brief writing to the ROM area.
  WritingToTheRomArea,
  /// @brief binary too large.
  BinaryTooLarge,
  /// @brief number too big.
  NumberTooBig
};

/// @brief エラーメッセージ表
static const std::unordered_map<ErrorCode, std::string> ErrorMessageTable{
    {ErrorCode::RegisterExpected, "レジスタ名が必要です。"},
    {ErrorCode::InvalidRegister, "レジスタ名が不正です。"},
    {ErrorCode::HExpected, "16進数リテラルには、末尾に 'H' が必要です。"},
    {ErrorCode::RPExpected, "')' （閉じ括弧） が必要です。"},
    {ErrorCode::InvalidCharLit, "文字定数が不正です。"},
    {ErrorCode::SingleQuotationExpected,
     "'\\'' （シングルクォーテーション） が必要です。"},
    {ErrorCode::ExpressionExpected, "数式が必要です。"},
    {ErrorCode::DoubleQuotationExpected,
     "'\\\"' （ダブルクォーテーション）が必要です。"},
    {ErrorCode::UndefinedLabel, "ラベルが定義されていません。"},
    {ErrorCode::ZeroDivision, "ゼロ除算が検出されました。"},
    {ErrorCode::UnknownInstruction, "オペコードが不正です。"},
    {ErrorCode::CommaExpected, "',' （コンマ）が必要です。"},
    {ErrorCode::IndexRegisterExpected, "インデクスレジスタが必要です。"},
    {ErrorCode::InvalidIndexRegister, "インデクスレジスタ名が不正です。"},
    {ErrorCode::InvalidImmediate, "即値は使用できません。"},
    {ErrorCode::InvalidOperand, "オペランドが不正です。"},
    {ErrorCode::InvalidLabel, "ラベルが不正です。"},
    {ErrorCode::DuplicatedLabel, "ラベルが重複しています。"},
    {ErrorCode::InvalidOrg,
     "ORG命令で、遡るアドレスを指定することはできません。"}};

static const std::unordered_map<WarningCode, std::string> WarningMessageTable{
    {WarningCode::IOAddressOutOfRange, "IOアドレスが範囲外です。"},
    {WarningCode::AddressOutOfRange, "アドレスが範囲外です。"},
    {WarningCode::ValueOutOfRange, "値が範囲外です。"},
    {WarningCode::WritingToTheRomArea, "ROM領域に書き込むことはできません。"},
    {WarningCode::BinaryTooLarge, "バイナリサイズが大きすぎます。"},
    {WarningCode::NumberTooBig, "数値が大きすぎます。"}};

/// @brief エラーを出力する。
static inline void
PrintError(const ErrorCode code, const size_t errBegin,
           const size_t errN = std::string::npos,
           const std::optional<std::string> &suggestion = std::nullopt) {
  std::string msg = std::format(
      "{}行目:\e[31mエラー\e[0m: {} （エラーコード: {}）\n", CurLineNum,
      ErrorMessageTable.at(code), static_cast<unsigned int>(code));
  assert(CurLineNum != 0 && Lines[CurLineNum - 1] == CurLine);
  if (CurLineNum != 1) {
    msg += std::format("{:>3}| {}\n", CurLineNum - 1, Lines[CurLineNum - 2]);
  }
  msg +=
      std::format("{:>3}| {}\e[31m{}\e[0m", CurLineNum,
                  CurLine.substr(0, errBegin), CurLine.substr(errBegin, errN));
  if (errN != std::string::npos) {
    assert(errBegin + errN <= CurLine.size());
    msg += CurLine.substr(errBegin + errN);
  }
  if (CurLineNum != Lines.size()) {
    msg += std::format("\n{:>3}| {}", CurLineNum + 1, Lines[CurLineNum]);
  }
  if (suggestion) {
    msg += '\n';
    msg += suggestion.value();
  }
  PrintError(msg);
}

static inline void
PrintWarning(const WarningCode code,
             const std::optional<std::string> &suggestion = std::nullopt) {
  std::string msg = std::format("\e[33m警告\e[0m: {} （警告コード: {}）",
                                WarningMessageTable.at(code),
                                static_cast<unsigned int>(code));
  if (suggestion) {
    msg += '\n';
    msg += suggestion.value();
  }
  PrintWarning(msg);
}

/// @brief ROMの開始アドレス
static constexpr uint8_t ROMStartAddr = 0xE0;

/// @brief 警告を出力する。
static inline void
PrintWarning(const WarningCode code, const size_t warningBeginIdx,
             const size_t warningN = std::string::npos,
             const std::optional<std::string> &suggestion = std::nullopt) {
  std::string msg = std::format(
      "{}行目:\e[33m警告\e[0m: {} （警告コード: {}）\n", CurLineNum,
      WarningMessageTable.at(code), static_cast<unsigned int>(code));
  assert(CurLineNum != 0 && Lines[CurLineNum - 1] == CurLine);
  if (CurLineNum != 1) {
    msg += std::format("{:>3}| {}\n", CurLineNum - 1, Lines[CurLineNum - 2]);
  }
  msg += std::format("{:>3}| {}\e[31m{}\e[0m", CurLineNum,
                     CurLine.substr(0, warningBeginIdx),
                     CurLine.substr(warningBeginIdx, warningN));
  if (warningN != std::string::npos) {
    assert(warningBeginIdx + warningN <= CurLine.size());
    msg += CurLine.substr(warningBeginIdx + warningN);
  }
  if (CurLineNum != Lines.size()) {
    msg += std::format("\n{:>3}| {}", CurLineNum + 1, Lines[CurLineNum]);
  }
  if (suggestion) {
    msg += '\n';
    msg += suggestion.value();
  }
  PrintWarning(msg);
}

/// @brief 現在の文字が10進数文字であるか判定する。
/// @return 10進数文字なら true, そうでなければ false
[[nodiscard]] static inline bool IsDigit() {
  return CurIdx < CurLine.size() && std::isdigit(CurLine[CurIdx]);
}

/// @brief 現在の文字が16進数文字であるか判定する。
/// @return 16進数文字なら true, そうでなければ false
[[nodiscard]] static inline bool IsXDigit() {
  return CurIdx < CurLine.size() && std::isxdigit(CurLine[CurIdx]);
}

/// @brief 数値を解析して読み飛ばす。
/// @return 解析が成功すれば true, そうでなければ false
[[nodiscard]] static inline bool ParseNum() {
  assert(IsDigit());
  bool isHex = false;
  const size_t numBegIdx = CurIdx;
  do {
    if (not IsDigit()) {
      isHex = true;
    }
    ++CurIdx;
  } while (IsXDigit());
  if (IsCh('H') || IsCh('h')) {
    isHex = true;
  } else if (isHex) {
    PrintError(ErrorCode::HExpected, numBegIdx, CurIdx - numBegIdx);
    return false;
  }
  return true;
}

// 再帰呼び出しのため
static bool ParseAdd();

/// @brief 値を解析して読み飛ばす。
/// @return 解析が成功すれば true, そうでなければ false
[[nodiscard]] static bool ParseVal() {
  SkipSpace();
  if (IsCh('+') || IsCh('-')) {
    SkipSpace();
  }
  // 値が始まった場所（エラーメッセージ用）
  const size_t valBegIdx = CurIdx;
  if (IsCh('(')) {
    if (not ParseAdd()) {
      return false;
    }
    if (not IsCh(')')) {
      PrintError(ErrorCode::RPExpected, valBegIdx, CurIdx - valBegIdx);
      return false;
    }
  } else if (IsCh('\'')) {
    if (CurLine.size() <= CurIdx || (not std::isprint(CurLine[CurIdx])) ||
        CurLine[CurIdx] == '\'') {
      PrintError(ErrorCode::InvalidCharLit, valBegIdx, CurIdx - valBegIdx);
      return false;
    }
    ++CurIdx; // 文字
    if (not IsCh('\'')) {
      PrintError(ErrorCode::SingleQuotationExpected, valBegIdx,
                 CurIdx - valBegIdx);
      return false;
    }
  } else if (IsDigit()) {
    if (not ParseNum()) {
      return false;
    }
  } else if (IsNameStart()) {
    ParseName();
  } else {
    PrintError(ErrorCode::ExpressionExpected, valBegIdx);
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
    SkipSpace();
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
    SkipSpace();
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
  SkipSpace();
  const size_t exprBegIdx = CurIdx;
  if (IsCh('"')) {
    while (CurIdx < CurLine.size() && std::isprint(CurLine[CurIdx]) &&
           CurLine[CurIdx] != '"') {
      ++count;
      ++CurIdx;
    }
    if (not IsCh('"')) {
      PrintError(ErrorCode::DoubleQuotationExpected, exprBegIdx,
                 CurIdx - exprBegIdx);
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
    SkipSpace();
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

/// @brief 16進数（大文字）の1文字をint32_t型に変換する。
/// @param ch 文字
/// @return int32_t型に変換された16進数の1文字
[[nodiscard]] static inline int32_t HexToInt(const char ch) noexcept {
  assert(('0' <= ch && ch <= '9') || ('A' <= ch && ch <= 'F'));
  if ('A' <= ch && ch <= 'F') {
    return ch - 'A' + 0xA;
  }
  return ch - '0';
}

/// @brief 数値を読み取る。
/// @param val 読み取った値
/// @return 読み取りが成功すれば true, そうでなければ false
[[nodiscard]] static bool GetNum(int32_t &val) {
  assert(CurIdx < CurLine.size() && std::isdigit(CurLine[CurIdx]));
  bool isHex = false;
  std::string numStr{};
  const size_t numBegIdx = CurIdx;
  do {
    if (not std::isdigit(CurLine[CurIdx])) {
      isHex = true;
    }
    numStr += std::toupper(CurLine[CurIdx++]);
  } while (CurIdx < CurLine.size() && std::isxdigit(CurLine[CurIdx]));
  if (IsCh('H') || IsCh('h')) {
    isHex = true;
  } else if (isHex) {
    PrintError(ErrorCode::HExpected, numBegIdx, CurIdx - numBegIdx);
    return false;
  }
  val = 0;
  // 一度符号なしで計算してから符号付きに変換する
  // (符号付きのオーバーフローは未定義動作のため)
  uint32_t unsignedVal = 0;
  bool overflow = false;
  if (isHex) {
    for (const char ch : numStr) {
      if (static_cast<uint32_t>(
              (std::numeric_limits<int32_t>::max() - HexToInt(ch)) >> 4) <
          unsignedVal) {
        overflow = true;
      }
      unsignedVal = (unsignedVal << 4) + HexToInt(ch);
    }
  } else {
    for (const char ch : numStr) {
      if (static_cast<uint32_t>(
              (std::numeric_limits<int32_t>::max() - (ch - '0')) / 10) <
          unsignedVal) {
        overflow = true;
      }
      unsignedVal = unsignedVal * 10 + ch - '0';
    }
  }
  val = static_cast<int32_t>(unsignedVal);
  if (overflow) {
    PrintWarning(WarningCode::NumberTooBig, numBegIdx, CurIdx - numBegIdx,
                 std::format("数値: {}", numStr + (isHex ? "H" : "")));
  }
  return true;
}

// 再帰呼び出しのため
static bool GetAdd(int32_t &val);

/// @brief 値を読みとる。
/// @param val 読み取った値
/// @return 解析が成功すれば true, そうでなければ false
[[nodiscard]] static bool GetVal(int32_t &val) {
  SkipSpace();
  bool pos = true;
  if (IsCh('+')) {
    SkipSpace();
  } else if (IsCh('-')) {
    SkipSpace();
    pos = false;
  }
  // 値が始まった場所（エラーメッセージ用）
  const size_t valBegIdx = CurIdx;
  if (IsCh('(')) {
    if (not GetAdd(val)) {
      return false;
    }
    if (not IsCh(')')) {
      PrintError(ErrorCode::RPExpected, valBegIdx, CurIdx - valBegIdx);
      return false;
    }
  } else if (IsCh('\'')) {
    if (CurLine.size() <= CurIdx || not std::isprint(CurLine[CurIdx]) ||
        CurLine[CurIdx] == '\'') {
      PrintError(ErrorCode::InvalidCharLit, valBegIdx, CurIdx - valBegIdx);
      return false;
    }
    val = CurLine[CurIdx++];
    if (not IsCh('\'')) {
      PrintError(ErrorCode::SingleQuotationExpected, valBegIdx,
                 CurIdx - valBegIdx);
      return false;
    }
  } else if (IsDigit()) {
    if (not GetNum(val)) {
      return false;
    }
  } else if (IsNameStart()) {
    std::string label = GetName();
    if (const auto labelIt = Labels.find(label); labelIt != Labels.end()) {
      val = labelIt->second.first;
    } else {
      PrintError(ErrorCode::UndefinedLabel, valBegIdx, CurIdx - valBegIdx,
                 std::format("ラベル: \"{}\"", label));
      return false;
    }
  } else {
    PrintError(ErrorCode::ExpressionExpected, valBegIdx);
    return false;
  }
  if (not pos) {
    val *= -1;
  }
  return true;
}

/// @brief 乗除算を読み取る。
/// @param val 読み取った値
/// @return 解析が成功すれば true, そうでなければ false
[[nodiscard]] static bool getMul(int32_t &val) {
  if (not GetVal(val)) {
    return false;
  }
  for (;;) {
    SkipSpace();
    const size_t divBegIdx = CurIdx;
    if (IsCh('*')) {
      int32_t rVal;
      if (not GetVal(rVal)) {
        return false;
      }
      val *= rVal;
    } else if (IsCh('/')) {
      int32_t rVal;
      if (not GetVal(rVal)) {
        return false;
      }
      if (rVal == 0x00) {
        PrintError(ErrorCode::ZeroDivision, divBegIdx, CurIdx - divBegIdx);
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
[[nodiscard]] static bool GetAdd(int32_t &val) {
  if (not getMul(val)) {
    return false;
  }
  for (;;) {
    SkipSpace();
    if (IsCh('+')) {
      int32_t rVal;
      if (not getMul(rVal)) {
        return false;
      }
      val += rVal;
    } else if (IsCh('-')) {
      int32_t rVal;
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
  SkipSpace();
  const size_t exprBegIdx = CurIdx;
  if (IsCh('"')) {
    while (CurIdx < CurLine.size() && std::isprint(CurLine[CurIdx]) &&
           CurLine[CurIdx] != '"') {
      binary[curAddr++] = CurLine[CurIdx++];
    }
    if (not IsCh('"')) {
      PrintError(ErrorCode::DoubleQuotationExpected, exprBegIdx,
                 CurIdx - exprBegIdx);
      return false;
    }
  } else {
    int32_t value = 0;
    const size_t valueBeginIdx = CurIdx;
    if (not GetAdd(value)) {
      return false;
    }
    if (value < -256 || 0xFF < value) {
      PrintWarning(WarningCode::ValueOutOfRange, valueBeginIdx,
                   CurIdx - valueBeginIdx,
                   std::format("範囲外の値: {}", value));
    }
    binary[curAddr++] = static_cast<uint8_t>(value);
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
    SkipSpace();
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
    // ラベルは必ず行頭
    assert(CurIdx == 0);
    label = GetName();
    if (const auto it = Labels.find(label); it != Labels.end()) {
      const size_t lineNum = it->second.second;
      assert(lineNum != 0);
      std::string msg =
          std::format("重複したラベル: \"{}\"\n以前の定義\n", label);
      if (lineNum != 1) {
        msg += std::format("{:>3}| {}\n", lineNum - 1, Lines[lineNum - 2]);
      }
      assert((not Lines[lineNum - 1].empty()) &&
             (std::isalpha(Lines[lineNum - 1].front()) ||
              Lines[lineNum - 1].front() == '_'));
      size_t i = 0;
      do {
        ++i;
      } while (i < Lines[lineNum - 1].size() &&
               (std::isalpha(Lines[lineNum - 1][i] ||
                             Lines[lineNum - 1][i] == '_')));
      msg += std::format("{:>3}| \e[33m{}\e[0m{}", lineNum,
                         Lines[lineNum - 1].substr(0, i),
                         Lines[lineNum - 1].substr(i));
      if (lineNum != Lines.size()) {
        msg += std::format("\n{:>3}| {}", lineNum + 1, Lines[lineNum]);
      }
      // ラベルは必ず行頭から始まるため 0 から CurIdx 文字
      PrintError(ErrorCode::DuplicatedLabel, 0, CurIdx, msg);
    }
  } else if (not IsSpaceOrComment()) {
    PrintError(
        ErrorCode::InvalidLabel, 0, std::string::npos,
        (CurIdx < CurLine.size() && (std::isprint(CurLine[CurIdx])))
            ? std::optional<
                  std::string>{"ラベルは、英字または、'_'"
                               "（アンダースコア）で始まる必要があります。"}
            : std::nullopt);
    return;
  }
  // ラベルの値
  uint8_t labelNum = curAddr;
  SkipSpace();
  if (IsNameStart()) {
    const size_t nameBegIdx = CurIdx;
    const std::string inst = GetName();
    if (inst == "EQU") {
      int32_t val = 0x00;
      const size_t valueBeginIdx = CurIdx;
      if (not GetAdd(val)) {
        return;
      }
      if (val < -256 || 0xFF < val) {
        PrintWarning(WarningCode::ValueOutOfRange, valueBeginIdx,
                     CurIdx - valueBeginIdx,
                     std::format("範囲外の値: {}", val));
      }
      labelNum = static_cast<uint8_t>(val);
    } else if (inst == "ORG") {
      int32_t val = 0x00;
      const size_t addrBegIdx = CurIdx;
      if (not GetAdd(val)) {
        return;
      }
      if (val < curAddr) {
        std::string msg = std::format(
            "（現在のアドレス: {:0>3X}H, 指定されたアドレス: {:0>3X}H）",
            curAddr & 0xFF, val & 0xFF);
        PrintError(ErrorCode::InvalidOrg, addrBegIdx, CurIdx - addrBegIdx, msg);
        return;
      }
      labelNum = val;
      curAddr = val;
    } else if (inst == "DS") {
      int32_t val = 0x00;
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
      CurIdx = CurLine.size();
    } else {
      std::string suggestion = std::format("オペコード: {}", inst);
      if (InstList.contains(label)) {
        suggestion +=
            std::format("\n"
                        "ラベル（\"{}\"）がオペコードと一致しています。\n"
                        "ラベルのない行には、行頭に空白またはタブが必要です。",
                        label);
      }
      PrintError(ErrorCode::UnknownInstruction, nameBegIdx, CurIdx - nameBegIdx,
                 suggestion);
      return;
    }
  }
  // ラベルがあればアドレスを登録
  if (not label.empty()) {
    Labels.emplace(label, std::make_pair(labelNum, CurLineNum));
  }
}

/// @brief パス1（ラベルの割り当てなど）を実行する。
static inline void Pass1() {
  uint8_t curAddr = 0x00;
  CurLineNum = 0;
  while (CurLineNum < Lines.size()) {
    // 行を進める（行番号は最初の行が1）
    CurLine = Lines[CurLineNum++];
    CurIdx = 0;
    Pass1Line(curAddr);
    // 読み終わった行を追加
  }
  // エラー発生ならここで中止
  CheckError();
}

/// @brief 機械語列用の配列型
using BinaryT = std::array<uint8_t, 256>;

/// @brief レジスタ部分を読む。
[[nodiscard]] static inline std::optional<GR> GetReg() {
  if (not IsNameStart()) {
    PrintError(ErrorCode::RegisterExpected, CurIdx);
    return std::nullopt;
  }
  const size_t regNameBeg = CurIdx;
  const std::string reg = GetName();
  if (reg == "G0") {
    return GR::G0;
  } else if (reg == "G1") {
    return GR::G1;
  } else if (reg == "G2") {
    return GR::G2;
  } else if (reg == "SP") {
    return GR::SP;
  }
  PrintError(ErrorCode::InvalidRegister, regNameBeg, CurIdx - regNameBeg,
             std::format("存在しないレジスタ名: \"{}\"", reg));
  return std::nullopt;
}

/// @brief インデクスレジスタ部分を読む。
[[nodiscard]] static inline std::optional<XR> GetIdxReg() {
  if (not IsNameStart()) {
    PrintError(ErrorCode::IndexRegisterExpected, CurIdx);
    return std::nullopt;
  }
  const size_t idxRegBeg = CurIdx;
  const std::string idxReg = GetName();
  if (idxReg == "G1") {
    return XR::G1Idx;
  } else if (idxReg == "G2") {
    return XR::G2Idx;
  }
  std::string msg =
      std::format("存在しないインデクスレジスタ名: \"{}\"", idxReg);
  if (idxReg == "G0" || idxReg == "SP") {
    msg += "\nインデクスレジスタとして使用できるのは、G1・G2のみです。";
  }
  PrintError(ErrorCode::InvalidIndexRegister, idxRegBeg, CurIdx - idxRegBeg,
             msg);
  return std::nullopt;
}

[[nodiscard]] static inline std::optional<uint8_t> GetAddress() {
  const size_t addrBeginIdx = CurIdx;
  int32_t addr = 0;
  if (not GetAdd(addr)) {
    return std::nullopt;
  }
  if (addr < -128 || 0xFF < addr) {
    PrintWarning(WarningCode::AddressOutOfRange, addrBeginIdx,
                 CurIdx - addrBeginIdx,
                 std::format("範囲外のアドレス: {}", addr));
  }
  return static_cast<uint8_t>(addr);
}

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
  SkipSpace();
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
      int32_t val = 0;
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
      int32_t val = 0;
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
      // 命令のタイプごとで分岐
      if (std::holds_alternative<InstType1>(instIt->second)) {
        // タイプ1の命令（オペランドなし）
        const InstType1 &inst1 = std::get<InstType1>(instIt->second);
        inst1.getBin(binary, curAddr);
      } else if (std::holds_alternative<InstType2>(instIt->second)) {
        // タイプ2の命令
        const InstType2 &inst2 = std::get<InstType2>(instIt->second);
        // 空白を読み飛ばす
        SkipSpace();
        // レジスタ
        if (const auto gr = GetReg()) {
          inst2.getBin(binary, curAddr, gr.value());
        } else {
          return;
        }
      } else if (std::holds_alternative<InstType3>(instIt->second)) {
        // タイプ3の命令 (IN or OUT)
        const InstType3 &inst3 = std::get<InstType3>(instIt->second);
        // 空白を読み飛ばす
        SkipSpace();
        // レジスタ
        GR gr = GR::G0;
        if (const auto optGR = GetReg()) {
          gr = optGR.value();
        } else {
          return;
        }
        // 空白を読み飛ばす
        SkipSpace();
        // ',' が必要
        if (not IsCh(',')) {
          PrintError(
              ErrorCode::CommaExpected, CurIdx, std::string::npos,
              (CurIdx == CurLine.size())
                  ? std::optional<std::string>{std::format(
                        "{}命令は、IOアドレスを指定する必要があります。", inst)}
                  : std::nullopt);
          return;
        }
        // アドレス
        int32_t addr = 0x00;
        const size_t addrBegIdx = CurIdx;
        if (not GetAdd(addr)) {
          return;
        }
        // IOアドレスが範囲外なら警告
        if (addr < 0 || 0x10 <= addr) {
          PrintWarning(
              WarningCode::IOAddressOutOfRange, addrBegIdx, CurIdx - addrBegIdx,
              std::format("範囲外のIOアドレス: {:0>3X}H", addr & 0xFF));
        }
        inst3.getBin(binary, curAddr, gr, static_cast<uint8_t>(addr & 0xFF));
      } else if (std::holds_alternative<InstType4>(instIt->second)) {
        // タイプ4の命令
        const InstType4 &inst4 = std::get<InstType4>(instIt->second);
        SkipSpace();
        // レジスタ
        GR gr = GR::G0;
        if (const auto optGR = GetReg()) {
          gr = optGR.value();
        } else {
          return;
        }
        // 空白を読み飛ばす
        SkipSpace();
        // ',' が必要
        if (not IsCh(',')) {
          PrintError(ErrorCode::CommaExpected, CurIdx);
          return;
        }
        // 空白を読み飛ばす
        SkipSpace();
        // アドレッシングモード
        XR xr = XR::Direct;
        // アドレス
        uint8_t addr = 0;
        if (IsCh('#')) {
          // '#' があれば即値
          xr = XR::Imm;
          if (const auto optAddr = GetAddress()) {
            addr = optAddr.value();
          } else {
            return;
          }
        } else {
          if (const auto optAddr = GetAddress()) {
            addr = optAddr.value();
          } else {
            return;
          }
          // 空白を読み飛ばす
          SkipSpace();
          if (IsCh(',')) {
            // ',' があればインデックスモード
            // 空白を読み飛ばす
            SkipSpace();
            if (const auto idxReg = GetIdxReg()) {
              xr = idxReg.value();
            } else {
              return;
            }
          }
        }
        inst4.getBin(binary, curAddr, gr, xr, addr);
      } else if (std::holds_alternative<InstType5>(instIt->second)) {
        // タイプ5の命令 (ST)
        const InstType5 &inst5 = std::get<InstType5>(instIt->second);
        SkipSpace();
        GR gr = GR::G0;
        if (const auto optGR = GetReg()) {
          gr = optGR.value();
        } else {
          return;
        }
        // 空白を読み飛ばす
        SkipSpace();
        // ',' が必要
        if (not IsCh(',')) {
          PrintError(ErrorCode::CommaExpected, CurIdx);
          return;
        }
        // 空白を読み飛ばす
        SkipSpace();
        // タイプ5の命令で即値は使用不可
        if (IsCh('#')) {
          PrintError(ErrorCode::InvalidImmediate, CurIdx - 1);
          return;
        }
        // アドレス
        uint8_t addr = 0x00;
        const size_t addrBeginIdx = CurIdx;
        if (const auto optAddr = GetAddress()) {
          addr = optAddr.value();
        } else {
          return;
        }
        const size_t addrN = CurIdx - addrBeginIdx;
        // 空白を読み飛ばす
        SkipSpace();
        // アドレッシングモード
        XR xr = XR::Direct;
        if (IsCh(',')) {
          // ',' があればインデックスモード
          SkipSpace();
          if (const auto optXR = GetIdxReg()) {
            xr = optXR.value();
          } else {
            return;
          }
        } else if (ROMStartAddr <= addr) {
          // ダイレクトアドレシングモードでROM領域への書き込み
          // Type5の命令は（今のところ）ST命令しかない
          PrintWarning(
              WarningCode::WritingToTheRomArea, addrBeginIdx, addrN,
              std::format("書き込み先アドレスとして、"
                          "{:0>3X}H番地が指定されています。\n"
                          "{:0>3X}H番地以降はROM領域のため、"
                          "この命令を実行しても主記憶上の値は変更されません。",
                          addr & 0xFF, ROMStartAddr & 0xFF));
        }
        inst5.getBin(binary, curAddr, gr, xr, addr);
      } else if (std::holds_alternative<InstType6>(instIt->second)) {
        // タイプ6の命令
        const InstType6 &inst6 = std::get<InstType6>(instIt->second);
        uint8_t addr = 0;
        if (const auto optAddr = GetAddress()) {
          addr = optAddr.value();
        } else {
          return;
        }
        // 空白を読み飛ばす
        SkipSpace();
        XR xr = XR::Direct;
        if (IsCh(',')) {
          // ',' があればインデックスモード
          // 空白を読み飛ばす
          SkipSpace();
          if (const auto optXR = GetIdxReg()) {
            xr = optXR.value();
          } else {
            return;
          }
        }
        inst6.getBin(binary, curAddr, xr, addr);
      } else {
        BUG("unknown instruction.");
      }
    } else {
      BUG("unknown instruction.");
      return;
    }
  }
  // 空白とコメントを読み飛ばす
  SkipSpaceOrComment();
  // 不正なオペランドが残っていたらエラー
  if (CurIdx < CurLine.size()) {
    PrintError(ErrorCode::InvalidOperand, CurIdx);
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
  CurLineNum = 0;
  // 1行づつ処理
  while (CurLineNum < Lines.size()) {
    // 行を進める
    CurLine = Lines[CurLineNum++];
    // 文字の添え字を初期化
    CurIdx = 0;
    // 1行分の処理
    Pass2Line(start, curAddr, binary);
  }
  // ROM領域にかかっている場合
  if (ROMStartAddr < curAddr) {
    PrintWarning(
        WarningCode::BinaryTooLarge,
        std::format(
            "プログラムは、{:0>3X}H番地まで使用しています。\n"
            "{:0>3X}H番地以降はROM領域のため、プログラムを書き込めません。",
            (curAddr - 1) & 0xFF, ROMStartAddr & 0xFF));
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
      Error(std::format("ファイルが開けませんでした。 (パス: \"{}\")",
                        binaryFileName));
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
      Error(std::format("ファイルが開けませんでした。 (パス: \"{}\")",
                        nameTableFileName));
    }
    // 名前表を出力
    for (const auto &[label, addrAndLineNum] : Labels) {
      ofs << std::format("{:<8} 0{:0>2X}H\n", label + ':',
                         addrAndLineNum.first & 0xFF);
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
    Error("拡張子は、\"t7\" である必要があります。");
  }
  {
    // ファイルを開く
    std::ifstream ifs{argv[1]};
    // 開けない時はエラー
    if (not ifs) {
      Error(std::format("ファイルが開けませんでした。(パス: \"{}\")", argv[1]));
    }
    // ファイルをすべて読み取る
    for (std::string line; std::getline(ifs, line);) {
      std::swap(Lines.emplace_back(), line);
    }
  }
  // パス1を実行（アドレス解決）
  Pass1();
  // パス2を実行（機械語と名前表生成）
  Pass2(progname);
  return 0;
}
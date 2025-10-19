#include "StarflightInput.h"
#include "InputCoreTypes.h"
#include "Logging/LogMacros.h"

// Include emulator keyboard interface
#include "../Emulator/graphics.h"

DEFINE_LOG_CATEGORY_STATIC(LogStarflightInput, Log, All);

void FStarflightInput::PushKey(const FKey& Key, bool bShift, bool bCtrl, bool bAlt)
{
    uint16_t ScanCode = ConvertToDOSScanCode(Key, bShift, bCtrl, bAlt);
    if (ScanCode != 0)
    {
        GraphicsPushKey(ScanCode);
        UE_LOG(LogStarflightInput, Verbose, TEXT("Pushed key: %s -> scan code %d"), *Key.ToString(), ScanCode);
    }
}

void FStarflightInput::PushRawScanCode(uint16_t ScanCode)
{
    if (ScanCode != 0)
    {
        GraphicsPushKey(ScanCode);
        UE_LOG(LogStarflightInput, Verbose, TEXT("Pushed raw scan code: %d"), ScanCode);
    }
}

bool FStarflightInput::HasKey()
{
    return GraphicsHasKey();
}

uint16_t FStarflightInput::ConvertToDOSScanCode(const FKey& Key, bool bShift, bool bCtrl, bool bAlt)
{
    // DOS BIOS keyboard scan codes
    // Format: Low byte = ASCII character, High byte = scan code
    // For extended keys (arrows, function keys), low byte = 0
    
    // Arrow keys (extended scan codes)
    if (Key == EKeys::Up) return 328;        // 0x4800
    if (Key == EKeys::Down) return 336;      // 0x5000
    if (Key == EKeys::Left) return 331;      // 0x4B00
    if (Key == EKeys::Right) return 333;     // 0x4D00
    
    // Numpad arrows
    if (Key == EKeys::NumPadEight) return 328;  // Up
    if (Key == EKeys::NumPadTwo) return 336;    // Down
    if (Key == EKeys::NumPadFour) return 331;   // Left
    if (Key == EKeys::NumPadSix) return 333;    // Right
    
    // Numpad diagonals
    if (Key == EKeys::NumPadSeven) return 327;  // Up-Left
    if (Key == EKeys::NumPadNine) return 329;   // Up-Right
    if (Key == EKeys::NumPadOne) return 335;    // Down-Left
    if (Key == EKeys::NumPadThree) return 337;  // Down-Right
    
    // Function keys (extended scan codes)
    if (Key == EKeys::F1) return 315;   // 0x3B00
    if (Key == EKeys::F2) return 316;   // 0x3C00
    if (Key == EKeys::F3) return 317;   // 0x3D00
    if (Key == EKeys::F4) return 318;   // 0x3E00
    if (Key == EKeys::F5) return 319;   // 0x3F00
    if (Key == EKeys::F6) return 320;   // 0x4000
    if (Key == EKeys::F7) return 321;   // 0x4100
    if (Key == EKeys::F8) return 322;   // 0x4200
    if (Key == EKeys::F9) return 323;   // 0x4300
    if (Key == EKeys::F10) return 324;  // 0x4400
    
    // Special keys
    if (Key == EKeys::Enter) return 13;
    if (Key == EKeys::Escape) return 27;
    if (Key == EKeys::BackSpace) return 8;
    if (Key == EKeys::Tab) return 9;
    if (Key == EKeys::SpaceBar) return 32;
    
    // Home/End/PgUp/PgDn
    if (Key == EKeys::Home) return 327;   // Same as NumPad 7
    if (Key == EKeys::End) return 335;    // Same as NumPad 1
    if (Key == EKeys::PageUp) return 329; // Same as NumPad 9
    if (Key == EKeys::PageDown) return 337; // Same as NumPad 3
    
    // Delete/Insert
    if (Key == EKeys::Delete) return 339;
    if (Key == EKeys::Insert) return 338;
    
    // Letters (A-Z) - return ASCII value
    if (Key == EKeys::A) return bShift ? 'A' : 'a';
    if (Key == EKeys::B) return bShift ? 'B' : 'b';
    if (Key == EKeys::C) return bShift ? 'C' : 'c';
    if (Key == EKeys::D) return bShift ? 'D' : 'd';
    if (Key == EKeys::E) return bShift ? 'E' : 'e';
    if (Key == EKeys::F) return bShift ? 'F' : 'f';
    if (Key == EKeys::G) return bShift ? 'G' : 'g';
    if (Key == EKeys::H) return bShift ? 'H' : 'h';
    if (Key == EKeys::I) return bShift ? 'I' : 'i';
    if (Key == EKeys::J) return bShift ? 'J' : 'j';
    if (Key == EKeys::K) return bShift ? 'K' : 'k';
    if (Key == EKeys::L) return bShift ? 'L' : 'l';
    if (Key == EKeys::M) return bShift ? 'M' : 'm';
    if (Key == EKeys::N) return bShift ? 'N' : 'n';
    if (Key == EKeys::O) return bShift ? 'O' : 'o';
    if (Key == EKeys::P) return bShift ? 'P' : 'p';
    if (Key == EKeys::Q) return bShift ? 'Q' : 'q';
    if (Key == EKeys::R) return bShift ? 'R' : 'r';
    if (Key == EKeys::S) return bShift ? 'S' : 's';
    if (Key == EKeys::T) return bShift ? 'T' : 't';
    if (Key == EKeys::U) return bShift ? 'U' : 'u';
    if (Key == EKeys::V) return bShift ? 'V' : 'v';
    if (Key == EKeys::W) return bShift ? 'W' : 'w';
    if (Key == EKeys::X) return bShift ? 'X' : 'x';
    if (Key == EKeys::Y) return bShift ? 'Y' : 'y';
    if (Key == EKeys::Z) return bShift ? 'Z' : 'z';
    
    // Numbers (0-9) - return ASCII value
    if (Key == EKeys::Zero) return bShift ? ')' : '0';
    if (Key == EKeys::One) return bShift ? '!' : '1';
    if (Key == EKeys::Two) return bShift ? '@' : '2';
    if (Key == EKeys::Three) return bShift ? '#' : '3';
    if (Key == EKeys::Four) return bShift ? '$' : '4';
    if (Key == EKeys::Five) return bShift ? '%' : '5';
    if (Key == EKeys::Six) return bShift ? '^' : '6';
    if (Key == EKeys::Seven) return bShift ? '&' : '7';
    if (Key == EKeys::Eight) return bShift ? '*' : '8';
    if (Key == EKeys::Nine) return bShift ? '(' : '9';
    
    // Numpad numbers (when NumLock is on)
    if (Key == EKeys::NumPadZero) return '0';
    if (Key == EKeys::NumPadFive) return '5';
    
    // Punctuation
    if (Key == EKeys::Comma) return bShift ? '<' : ',';
    if (Key == EKeys::Period) return bShift ? '>' : '.';
    if (Key == EKeys::Slash) return bShift ? '?' : '/';
    if (Key == EKeys::Semicolon) return bShift ? ':' : ';';
    if (Key == EKeys::Apostrophe) return bShift ? '"' : '\'';
    if (Key == EKeys::LeftBracket) return bShift ? '{' : '[';
    if (Key == EKeys::RightBracket) return bShift ? '}' : ']';
    if (Key == EKeys::Backslash) return bShift ? '|' : '\\';
    if (Key == EKeys::Hyphen) return bShift ? '_' : '-';
    if (Key == EKeys::Equals) return bShift ? '+' : '=';
    if (Key == EKeys::Tilde) return bShift ? '~' : '`';
    
    UE_LOG(LogStarflightInput, Warning, TEXT("Unmapped key: %s"), *Key.ToString());
    return 0;
}


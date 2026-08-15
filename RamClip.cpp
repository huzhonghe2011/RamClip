#ifndef UNICODE
#define UNICODE
#endif

#ifndef _UNICODE
#define _UNICODE
#endif

#define WIN32_LEAN_AND_MEAN

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif

#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <d2d1.h>
#include <dwrite.h>

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#ifndef BI_ALPHABITFIELDS
#define BI_ALPHABITFIELDS 6
#endif

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")

static constexpr wchar_t kAppTitle[] = L"RamClip";

template<class T>
static void SafeRelease(T*& p) {
    if (p) {
        p->Release();
        p = nullptr;
    }
}

// CF_HDROP 开头的数据结构。
// 不依赖某些 LLVM-MinGW 头文件组合中缺失的系统文件投递头 typedef。
struct DropFilesHeader {
    DWORD pFiles;
    POINT pt;
    BOOL fNC;
    BOOL fWide;
};

static_assert(
    sizeof(DropFilesHeader) == 20,
    "Unexpected CF_HDROP header size"
);
static_assert(offsetof(DropFilesHeader, pFiles) == 0);
static_assert(offsetof(DropFilesHeader, pt) == 4);
static_assert(offsetof(DropFilesHeader, fNC) == 12);
static_assert(offsetof(DropFilesHeader, fWide) == 16);
static_assert(sizeof(wchar_t) == 2, "RamClip expects the Windows UTF-16 ABI");


struct ClipFormatData {
    UINT format = 0;
    std::vector<std::uint8_t> bytes;
};

struct PreviewImage {
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> bgra; // BGRA, alpha 255
    bool valid() const { return width > 0 && height > 0 && !bgra.empty(); }
};

enum class PreviewKind {
    Text,
    Image,
    Files,
    Generic
};

struct ClipSlot {
    std::vector<ClipFormatData> formats;
    PreviewKind previewKind = PreviewKind::Generic;
    std::wstring previewText;
    PreviewImage previewImage;
    std::wstring typeLabel;
};

struct LayoutItem {
    float y = 0;
    float h = 0;
    int slotIndex = -1;
};

struct ClipboardBackup {
    ClipSlot slot;
    bool valid = false;
    bool wasEmpty = true;
};

enum class PastePhase {
    None,
    RestoreClipboard
};

enum class CaptureSelectionPhase {
    None,
    WaitModifiers,
    WaitClipboard
};

static HWND g_hwnd = nullptr;
static HWND g_instructionHwnd = nullptr;
static HWND g_pasteTarget = nullptr;
static HWND g_captureTarget = nullptr;

static ID2D1Factory* g_d2dFactory = nullptr;
static IDWriteFactory* g_dwFactory = nullptr;
static ID2D1DCRenderTarget* g_dcTarget = nullptr;
static IDWriteTextFormat* g_textFormat = nullptr;
static IDWriteTextFormat* g_smallFormat = nullptr;

static HDC g_memDC = nullptr;
static HBITMAP g_dib = nullptr;
static HGDIOBJ g_oldBitmap = nullptr;
static void* g_dibBits = nullptr;
static int g_surfaceW = 0;
static int g_surfaceH = 0;

static HDC g_instructionMemDC = nullptr;
static HBITMAP g_instructionDib = nullptr;
static HGDIOBJ g_instructionOldBitmap = nullptr;
static void* g_instructionDibBits = nullptr;
static int g_instructionSurfaceW = 0;
static int g_instructionSurfaceH = 0;

static std::vector<ClipSlot> g_slots;
static std::vector<LayoutItem> g_layout;
static int g_selected = -1;

static float g_scale = 1.0f;
static float g_scrollY = 0.0f;
static float g_totalHeight = 0.0f;
static float g_maxScroll = 0.0f;

static int g_windowX = 0;
static int g_windowY = 0;
static int g_windowW = 420;
static int g_windowH = 300;

static int g_instructionX = 0;
static int g_instructionY = 0;
static int g_instructionW = 420;
static int g_instructionH = 300;

static int g_windowTargetX = 0;
static int g_windowTargetY = 0;
static int g_instructionTargetX = 0;
static int g_instructionTargetY = 0;

static float g_instructionAlpha = 0.90f;
static bool g_instructionHovered = false;

static std::vector<float> g_slotHoverAmount;
static int g_hoveredSlot = -1;

static float g_panelSlide = 0.0f;
static bool g_panelOpenTarget = false;

static RECT g_monitorRect{};

static bool g_panelVisible = false;
static bool g_panelHotkeysRegistered = false;
static HHOOK g_panelMouseHook = nullptr;

static constexpr UINT WM_APP_OUTSIDE_CLICK = WM_APP + 1;

static std::wstring g_status;

static PastePhase g_pastePhase = PastePhase::None;
static int g_pasteRestoreTicks = 0;
static int g_pasteRestoreDelayTicks = 25;
static DWORD g_pasteTempSequence = 0;
static ClipboardBackup g_pasteBackup;

static CaptureSelectionPhase g_captureSelectionPhase = CaptureSelectionPhase::None;
static int g_captureSelectionTicks = 0;
static DWORD g_captureSequenceBefore = 0;
static ClipboardBackup g_captureBackup;

enum HotkeyId {
    HK_TOGGLE = 100,
    HK_ADD = 101,
    HK_EXIT = 102,

    HK_CYCLE = 110,
    HK_COPY_CURRENT = 111,
    HK_DELETE = 112,
    HK_PASTE = 113,
    HK_PASTE_DELETE = 114,

    HK_GLOBAL_CAPTURE = 120,
    HK_GLOBAL_PASTE = 121,
    HK_GLOBAL_DELETE = 122
};

enum TimerId {
    TIMER_STATUS = 200,
    TIMER_PASTE = 201,
    TIMER_CAPTURE_SELECTION = 202,
    TIMER_UI_ANIMATION = 203,
    TIMER_TOPMOST_WATCH = 204
};

static const wchar_t* kInstructions =
    L"Alt+`：打开/关闭界面\n"
    L"剪贴板槽位位于右上角\n"
    L"点击可切换槽位\n"
    L"Alt+C：将当前选中内容添加为槽位 1\n"
    L"Alt+V：全局粘贴槽位 1（可连续触发）\n"
    L"Alt+Z：删除槽位 1\n"
    L"Alt+1：切换槽位\n"
    L"Alt+2：将当前剪贴板内容添加为槽位 1\n"
    L"Alt+3：复制当前槽位到系统剪贴板\n"
    L"Alt+4：删除当前槽位\n"
    L"Alt+5：全局粘贴当前槽位（可连续触发）\n"
    L"Ctrl+Alt+1：全局粘贴当前槽位并删除（可连续触发）\n"
    L"Ctrl+Alt+`：退出程序";

static bool OpenClipboardRetry(HWND owner) {
    for (int i = 0; i < 12; ++i) {
        if (OpenClipboard(owner)) return true;
        Sleep(8);
    }
    return false;
}

static const ClipFormatData* FindFormat(const ClipSlot& slot, UINT fmt) {
    for (const auto& f : slot.formats) {
        if (f.format == fmt) return &f;
    }
    return nullptr;
}

static std::wstring BytesToWideText(const std::vector<std::uint8_t>& b) {
    if (b.size() < sizeof(wchar_t)) return {};

    const size_t count = b.size() / sizeof(wchar_t);
    std::wstring out;
    out.reserve(std::min<size_t>(count, 4096));

    for (size_t i = 0; i < count; ++i) {
        wchar_t ch = L'\0';
        std::memcpy(
            &ch,
            b.data() + i * sizeof(wchar_t),
            sizeof(wchar_t)
        );
        if (ch == L'\0') break;
        out.push_back(ch);
    }

    return out;
}

static std::wstring AnsiToWide(const char* s, size_t len) {
    if (!s || len == 0) return {};
    int need = MultiByteToWideChar(CP_ACP, 0, s, static_cast<int>(len), nullptr, 0);
    if (need <= 0) return {};
    std::wstring out(static_cast<size_t>(need), L'\0');
    MultiByteToWideChar(CP_ACP, 0, s, static_cast<int>(len), out.data(), need);
    return out;
}

static std::wstring FileListPreview(
    const std::vector<std::uint8_t>& b,
    int* outCount = nullptr
) {
    if (outCount) {
        *outCount = 0;
    }

    // Some LLVM-MinGW header combinations do not expose the system file-drop header typedef.
    // Copy the compatible 20-byte header instead of aliasing byte storage.
    if (b.size() < sizeof(DropFilesHeader)) {
        return {};
    }

    DropFilesHeader df{};
    std::memcpy(&df, b.data(), sizeof(df));

    if (df.pFiles < sizeof(DropFilesHeader) ||
        static_cast<size_t>(df.pFiles) >= b.size()) {
        return {};
    }

    const size_t payloadOffset = static_cast<size_t>(df.pFiles);
    std::wstring out;
    int count = 0;
    constexpr int maxShown = 8;

    if (df.fWide) {
        const size_t payloadBytes = b.size() - payloadOffset;
        const size_t codeUnits = payloadBytes / sizeof(wchar_t);

        size_t pos = 0;
        while (pos < codeUnits) {
            std::wstring path;

            for (; pos < codeUnits; ++pos) {
                wchar_t ch = L'\0';
                std::memcpy(
                    &ch,
                    b.data() + payloadOffset +
                        pos * sizeof(wchar_t),
                    sizeof(wchar_t)
                );

                if (ch == L'\0') {
                    ++pos;
                    break;
                }

                path.push_back(ch);
            }

            if (path.empty()) {
                break;
            }

            ++count;
            if (count <= maxShown) {
                if (!out.empty()) out += L"\n";
                out += path;
            }
        }
    } else {
        const char* p = reinterpret_cast<const char*>(
            b.data() + payloadOffset
        );
        const size_t remain = b.size() - payloadOffset;
        size_t pos = 0;

        while (pos < remain) {
            const size_t start = pos;
            while (pos < remain && p[pos] != '\0') {
                ++pos;
            }

            if (pos == start) {
                break;
            }

            ++count;
            if (count <= maxShown) {
                if (!out.empty()) out += L"\n";
                out += AnsiToWide(p + start, pos - start);
            }

            if (pos < remain) ++pos;
        }
    }

    if (count > maxShown) {
        out += L"\n…";
    }

    if (outCount) {
        *outCount = count;
    }

    return out;
}

static bool CheckedAddSize(
    size_t a,
    size_t b,
    size_t& out
) {
    if (a > std::numeric_limits<size_t>::max() - b) {
        return false;
    }
    out = a + b;
    return true;
}

static bool CheckedMulSize(
    size_t a,
    size_t b,
    size_t& out
) {
    if (a != 0 &&
        b > std::numeric_limits<size_t>::max() / a) {
        return false;
    }
    out = a * b;
    return true;
}

static bool ReadBitmapInfoHeader(
    const std::vector<std::uint8_t>& b,
    BITMAPINFOHEADER& h
) {
    if (b.size() < sizeof(BITMAPINFOHEADER)) {
        return false;
    }

    std::memcpy(&h, b.data(), sizeof(h));

    if (h.biSize < sizeof(BITMAPINFOHEADER) ||
        static_cast<size_t>(h.biSize) > b.size()) {
        return false;
    }

    return true;
}

static size_t DIBBitsOffset(
    const std::vector<std::uint8_t>& b,
    const BITMAPINFOHEADER& h
) {
    size_t offset = static_cast<size_t>(h.biSize);

    if (h.biSize == sizeof(BITMAPINFOHEADER)) {
        size_t maskBytes = 0;
        if (h.biCompression == BI_BITFIELDS) {
            maskBytes = 3 * sizeof(DWORD);
        } else if (h.biCompression == BI_ALPHABITFIELDS) {
            maskBytes = 4 * sizeof(DWORD);
        }

        if (!CheckedAddSize(offset, maskBytes, offset)) {
            return 0;
        }
    }

    DWORD colors = h.biClrUsed;
    if (colors == 0 && h.biBitCount <= 8) {
        colors = 1u << h.biBitCount;
    }

    size_t paletteBytes = 0;
    if (!CheckedMulSize(
            static_cast<size_t>(colors),
            sizeof(RGBQUAD),
            paletteBytes
        ) ||
        !CheckedAddSize(offset, paletteBytes, offset)) {
        return 0;
    }

    if (offset >= b.size()) {
        return 0;
    }

    return offset;
}

static bool ValidateDIBPayload(
    const std::vector<std::uint8_t>& b,
    BITMAPINFOHEADER& h,
    size_t& bitsOffset,
    int& srcW,
    int& srcH
) {
    if (!ReadBitmapInfoHeader(b, h)) {
        return false;
    }

    if (h.biPlanes != 1 ||
        h.biWidth <= 0 ||
        h.biHeight == 0 ||
        h.biHeight == INT_MIN) {
        return false;
    }

    srcW = h.biWidth;
    srcH = h.biHeight < 0 ? -h.biHeight : h.biHeight;

    if (srcW > 32768 || srcH > 32768) {
        return false;
    }

    switch (h.biBitCount) {
    case 1:
    case 4:
    case 8:
    case 16:
    case 24:
    case 32:
        break;
    default:
        return false;
    }

    if (h.biCompression != BI_RGB &&
        h.biCompression != BI_BITFIELDS &&
        h.biCompression != BI_ALPHABITFIELDS) {
        return false;
    }

    if ((h.biCompression == BI_BITFIELDS ||
         h.biCompression == BI_ALPHABITFIELDS) &&
        h.biBitCount != 16 &&
        h.biBitCount != 32) {
        return false;
    }

    bitsOffset = DIBBitsOffset(b, h);
    if (bitsOffset == 0) {
        return false;
    }

    size_t rowBits = 0;
    if (!CheckedMulSize(
            static_cast<size_t>(srcW),
            static_cast<size_t>(h.biBitCount),
            rowBits
        )) {
        return false;
    }

    size_t rowBitsRounded = 0;
    if (!CheckedAddSize(rowBits, 31u, rowBitsRounded)) {
        return false;
    }

    size_t stride = (rowBitsRounded / 32u) * 4u;
    size_t pixelBytes = 0;
    if (!CheckedMulSize(
            stride,
            static_cast<size_t>(srcH),
            pixelBytes
        )) {
        return false;
    }

    if (pixelBytes > b.size() - bitsOffset) {
        return false;
    }

    if (h.biSizeImage != 0 &&
        static_cast<size_t>(h.biSizeImage) >
            b.size() - bitsOffset) {
        return false;
    }

    return true;
}

static PreviewImage PreviewFromDIB(const std::vector<std::uint8_t>& b) {
    PreviewImage out;

    BITMAPINFOHEADER h{};
    size_t bitsOffset = 0;
    int srcW = 0;
    int srcH = 0;

    if (!ValidateDIBPayload(
            b,
            h,
            bitsOffset,
            srcW,
            srcH
        )) {
        return out;
    }

    const int maxDim = 640;
    const double s = std::min(
        1.0,
        static_cast<double>(maxDim) /
            static_cast<double>(std::max(srcW, srcH))
    );
    const int dstW = std::max(
        1,
        static_cast<int>(std::lround(srcW * s))
    );
    const int dstH = std::max(
        1,
        static_cast<int>(std::lround(srcH * s))
    );

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = dstW;
    bmi.bmiHeader.biHeight = -dstH;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* pixels = nullptr;
    HDC dc = CreateCompatibleDC(nullptr);
    if (!dc) return out;

    HBITMAP bmp = CreateDIBSection(
        dc,
        &bmi,
        DIB_RGB_COLORS,
        &pixels,
        nullptr,
        0
    );
    if (!bmp || !pixels) {
        if (bmp) DeleteObject(bmp);
        DeleteDC(dc);
        return out;
    }

    HGDIOBJ old = SelectObject(dc, bmp);
    SetStretchBltMode(dc, HALFTONE);
    SetBrushOrgEx(dc, 0, 0, nullptr);

    const int copied = StretchDIBits(
        dc,
        0, 0, dstW, dstH,
        0, 0, srcW, srcH,
        b.data() + bitsOffset,
        reinterpret_cast<const BITMAPINFO*>(b.data()),
        DIB_RGB_COLORS,
        SRCCOPY
    );

    if (copied != GDI_ERROR && copied != 0) {
        out.width = dstW;
        out.height = dstH;
        out.bgra.resize(
            static_cast<size_t>(dstW) *
            static_cast<size_t>(dstH) *
            4u
        );
        std::memcpy(
            out.bgra.data(),
            pixels,
            out.bgra.size()
        );

        for (size_t i = 3; i < out.bgra.size(); i += 4) {
            out.bgra[i] = 255;
        }
    }

    SelectObject(dc, old);
    DeleteObject(bmp);
    DeleteDC(dc);
    return out;
}

static bool BitmapToDIBV5(HBITMAP hbmp, std::vector<std::uint8_t>& out) {
    if (!hbmp) return false;

    BITMAP bm{};
    if (!GetObjectW(hbmp, sizeof(bm), &bm)) return false;

    const int w = bm.bmWidth;
    const int h = std::abs(bm.bmHeight);
    if (w <= 0 || h <= 0) return false;

    const size_t pixelBytes = static_cast<size_t>(w) * h * 4;
    if (pixelBytes > 512ull * 1024ull * 1024ull) return false;

    out.resize(sizeof(BITMAPV5HEADER) + pixelBytes);
    std::memset(out.data(), 0, sizeof(BITMAPV5HEADER));

    auto* v5 = reinterpret_cast<BITMAPV5HEADER*>(out.data());
    v5->bV5Size = sizeof(BITMAPV5HEADER);
    v5->bV5Width = w;
    v5->bV5Height = -h;
    v5->bV5Planes = 1;
    v5->bV5BitCount = 32;
    v5->bV5Compression = BI_BITFIELDS;
    v5->bV5RedMask   = 0x00FF0000;
    v5->bV5GreenMask = 0x0000FF00;
    v5->bV5BlueMask  = 0x000000FF;
    v5->bV5AlphaMask = 0xFF000000;
    v5->bV5CSType = LCS_sRGB;
    v5->bV5SizeImage = static_cast<DWORD>(pixelBytes);

    HDC dc = GetDC(nullptr);
    if (!dc) {
        out.clear();
        return false;
    }

    int lines = GetDIBits(
        dc,
        hbmp,
        0,
        static_cast<UINT>(h),
        out.data() + sizeof(BITMAPV5HEADER),
        reinterpret_cast<BITMAPINFO*>(v5),
        DIB_RGB_COLORS
    );
    ReleaseDC(nullptr, dc);

    if (lines == 0) {
        out.clear();
        return false;
    }

    // DDBs often return zero alpha; make it opaque.
    auto* px = out.data() + sizeof(BITMAPV5HEADER);
    for (size_t i = 3; i < pixelBytes; i += 4) px[i] = 255;
    return true;
}

static std::wstring RegisteredFormatName(UINT fmt) {
    wchar_t name[256]{};
    int n = GetClipboardFormatNameW(fmt, name, 255);
    if (n > 0) return std::wstring(name, name + n);
    return {};
}

static bool IsGlobalMemoryClipboardFormat(UINT fmt) {
    switch (fmt) {
    case CF_BITMAP:
    case CF_PALETTE:
    case CF_METAFILEPICT:
    case CF_ENHMETAFILE:
    case CF_DSPBITMAP:
    case CF_DSPMETAFILEPICT:
    case CF_DSPENHMETAFILE:
        return false;
    default:
        break;
    }

    if (fmt >= CF_GDIOBJFIRST &&
        fmt <= CF_GDIOBJLAST) {
        return false;
    }

    return true;
}

static bool CaptureClipboard(ClipSlot& slot, std::wstring& error) {
    slot = ClipSlot{};
    error.clear();

    if (!OpenClipboardRetry(g_hwnd)) {
        error = L"无法打开剪贴板（可能正被其他程序占用）";
        return false;
    }

    std::unordered_set<UINT> captured;
    size_t totalCopied = 0;
    constexpr size_t kPerFormatLimit = 256ull * 1024ull * 1024ull;
    constexpr size_t kTotalLimit = 512ull * 1024ull * 1024ull;

    UINT fmt = 0;
    for (;;) {
        SetLastError(ERROR_SUCCESS);
        const UINT next = EnumClipboardFormats(fmt);

        if (next == 0) {
            const DWORD enumError = GetLastError();
            if (enumError != ERROR_SUCCESS) {
                CloseClipboard();
                slot = ClipSlot{};
                error =
                    L"枚举剪贴板格式失败（错误 " +
                    std::to_wstring(enumError) +
                    L"）";
                return false;
            }
            break;
        }

        fmt = next;

        if (!IsGlobalMemoryClipboardFormat(fmt)) {
            continue;
        }

        HANDLE h = GetClipboardData(fmt);
        if (!h) continue;

        SIZE_T sz = GlobalSize(static_cast<HGLOBAL>(h));
        if (sz == 0 ||
            sz > kPerFormatLimit ||
            totalCopied + sz > kTotalLimit) {
            continue;
        }

        void* p = GlobalLock(static_cast<HGLOBAL>(h));
        if (!p) continue;

        ClipFormatData data;
        data.format = fmt;
        data.bytes.resize(sz);
        std::memcpy(data.bytes.data(), p, sz);
        GlobalUnlock(static_cast<HGLOBAL>(h));

        totalCopied += sz;
        captured.insert(fmt);
        slot.formats.push_back(std::move(data));
    }

    // If the source only provides CF_BITMAP, convert it to a portable DIBV5 block.
    if (captured.find(CF_DIBV5) == captured.end() &&
        captured.find(CF_DIB) == captured.end() &&
        IsClipboardFormatAvailable(CF_BITMAP)) {
        HBITMAP hbmp = static_cast<HBITMAP>(GetClipboardData(CF_BITMAP));
        std::vector<std::uint8_t> dibv5;
        if (BitmapToDIBV5(hbmp, dibv5)) {
            ClipFormatData f;
            f.format = CF_DIBV5;
            f.bytes = std::move(dibv5);
            slot.formats.push_back(std::move(f));
            captured.insert(CF_DIBV5);
        }
    }

    CloseClipboard();

    if (slot.formats.empty()) {
        error = L"当前剪贴板没有可保存的数据";
        return false;
    }

    // Preview priority: real files -> image -> Unicode text -> generic.
    if (const ClipFormatData* files = FindFormat(slot, CF_HDROP)) {
        int count = 0;
        slot.previewText = FileListPreview(files->bytes, &count);
        slot.previewKind = PreviewKind::Files;
        slot.typeLabel = L"文件";
        if (count > 0) {
            slot.typeLabel += L" · " + std::to_wstring(count) + L" 项";
        }
        return true;
    }

    if (const ClipFormatData* dibv5 = FindFormat(slot, CF_DIBV5)) {
        slot.previewImage = PreviewFromDIB(dibv5->bytes);
        if (slot.previewImage.valid()) {
            slot.previewKind = PreviewKind::Image;
            slot.typeLabel = L"图片";
            return true;
        }
    }
    if (const ClipFormatData* dib = FindFormat(slot, CF_DIB)) {
        slot.previewImage = PreviewFromDIB(dib->bytes);
        if (slot.previewImage.valid()) {
            slot.previewKind = PreviewKind::Image;
            slot.typeLabel = L"图片";
            return true;
        }
    }

    if (const ClipFormatData* text = FindFormat(slot, CF_UNICODETEXT)) {
        slot.previewText = BytesToWideText(text->bytes);
        slot.previewKind = PreviewKind::Text;
        slot.typeLabel = L"文字";
        return true;
    }

    slot.previewKind = PreviewKind::Generic;
    slot.typeLabel = L"剪贴板数据 · " + std::to_wstring(slot.formats.size()) + L" 种格式";

    // Show a useful registered format name if available.
    for (const auto& f : slot.formats) {
        if (f.format >= 0xC000) {
            auto n = RegisteredFormatName(f.format);
            if (!n.empty()) {
                slot.previewText = n;
                break;
            }
        }
    }
    if (slot.previewText.empty()) slot.previewText = L"无可预览内容";
    return true;
}

static bool PutSlotOnClipboard(const ClipSlot& slot, std::wstring& error) {
    error.clear();
    if (slot.formats.empty()) {
        error = L"该槽位没有可用数据";
        return false;
    }

    if (!OpenClipboardRetry(g_hwnd)) {
        error = L"无法打开剪贴板";
        return false;
    }

    if (!EmptyClipboard()) {
        CloseClipboard();
        error = L"无法清空剪贴板";
        return false;
    }

    int successCount = 0;
    std::unordered_set<UINT> emitted;

    for (const auto& f : slot.formats) {
        if (f.bytes.empty()) continue;
        if (!emitted.insert(f.format).second) continue;

        HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, f.bytes.size());
        if (!h) continue;
        void* p = GlobalLock(h);
        if (!p) {
            GlobalFree(h);
            continue;
        }
        std::memcpy(p, f.bytes.data(), f.bytes.size());
        GlobalUnlock(h);

        if (SetClipboardData(f.format, h)) {
            ++successCount; // clipboard now owns h
        } else {
            GlobalFree(h);
        }
    }

    CloseClipboard();

    if (successCount == 0) {
        error = L"无法写入剪贴板";
        return false;
    }
    return true;
}


static bool SaveClipboardBackup(ClipboardBackup& backup, std::wstring& error) {
    backup = ClipboardBackup{};
    error.clear();

    SetLastError(ERROR_SUCCESS);
    const int formatCount = CountClipboardFormats();

    if (formatCount == 0) {
        const DWORD countError = GetLastError();
        if (countError != ERROR_SUCCESS) {
            error =
                L"无法查询系统剪贴板格式（错误 " +
                std::to_wstring(countError) +
                L"）";
            return false;
        }

        backup.valid = true;
        backup.wasEmpty = true;
        return true;
    }

    ClipSlot snapshot;
    if (!CaptureClipboard(snapshot, error)) {
        if (error.empty()) error = L"无法临时保存系统剪贴板";
        return false;
    }

    backup.slot = std::move(snapshot);
    backup.valid = true;
    backup.wasEmpty = false;
    return true;
}

static bool RestoreClipboardBackup(const ClipboardBackup& backup, std::wstring& error) {
    error.clear();
    if (!backup.valid) {
        error = L"没有可恢复的系统剪贴板快照";
        return false;
    }

    if (!backup.wasEmpty) {
        return PutSlotOnClipboard(backup.slot, error);
    }

    if (!OpenClipboardRetry(g_hwnd)) {
        error = L"无法打开剪贴板以恢复为空状态";
        return false;
    }

    const BOOL ok = EmptyClipboard();
    CloseClipboard();

    if (!ok) {
        error = L"无法恢复原来的空剪贴板";
        return false;
    }
    return true;
}

static void UpdatePasteTargetFromForeground() {
    HWND fg = GetForegroundWindow();
    if (fg && fg != g_hwnd && fg != g_instructionHwnd) {
        g_pasteTarget = fg;
    }
}

static bool WindowHasTopmostStyle(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) {
        return false;
    }

    const LONG_PTR exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    return (exStyle & WS_EX_TOPMOST) != 0;
}

static void SetWindowTopmostNoActivate(HWND hwnd, bool forceRefresh) {
    if (!hwnd || !IsWindow(hwnd)) {
        return;
    }

    // Normally only repair a window that was actually demoted from the
    // TOPMOST band. When the UI is opened, forceRefresh=true also moves
    // the overlay to the front of the current TOPMOST band without focus.
    if (!forceRefresh && WindowHasTopmostStyle(hwnd)) {
        return;
    }

    SetWindowPos(
        hwnd,
        HWND_TOPMOST,
        0,
        0,
        0,
        0,
        SWP_NOMOVE |
        SWP_NOSIZE |
        SWP_NOACTIVATE |
        SWP_NOSENDCHANGING
    );
}

static void EnsurePanelsTopmost(bool forceRefresh = false) {
    SetWindowTopmostNoActivate(g_hwnd, forceRefresh);
    SetWindowTopmostNoActivate(g_instructionHwnd, forceRefresh);
}


static void DestroySurface() {
    if (g_memDC) {
        if (g_oldBitmap) SelectObject(g_memDC, g_oldBitmap);
        if (g_dib) DeleteObject(g_dib);
        DeleteDC(g_memDC);
    }
    g_memDC = nullptr;
    g_dib = nullptr;
    g_oldBitmap = nullptr;
    g_dibBits = nullptr;
    g_surfaceW = g_surfaceH = 0;
}

static bool EnsureSurface(int w, int h) {
    if (w <= 0 || h <= 0) return false;
    if (g_memDC && g_surfaceW == w && g_surfaceH == h) return true;

    DestroySurface();

    HDC screen = GetDC(nullptr);
    g_memDC = CreateCompatibleDC(screen);
    ReleaseDC(nullptr, screen);
    if (!g_memDC) return false;

    BITMAPV5HEADER bh{};
    bh.bV5Size = sizeof(BITMAPV5HEADER);
    bh.bV5Width = w;
    bh.bV5Height = -h;
    bh.bV5Planes = 1;
    bh.bV5BitCount = 32;
    bh.bV5Compression = BI_BITFIELDS;
    bh.bV5RedMask = 0x00FF0000;
    bh.bV5GreenMask = 0x0000FF00;
    bh.bV5BlueMask = 0x000000FF;
    bh.bV5AlphaMask = 0xFF000000;
    bh.bV5CSType = LCS_sRGB;

    g_dib = CreateDIBSection(
        g_memDC,
        reinterpret_cast<BITMAPINFO*>(&bh),
        DIB_RGB_COLORS,
        &g_dibBits,
        nullptr,
        0
    );
    if (!g_dib || !g_dibBits) {
        DestroySurface();
        return false;
    }

    g_oldBitmap = SelectObject(g_memDC, g_dib);
    g_surfaceW = w;
    g_surfaceH = h;
    return true;
}


static void DestroyInstructionSurface() {
    if (g_instructionMemDC) {
        if (g_instructionOldBitmap) {
            SelectObject(g_instructionMemDC, g_instructionOldBitmap);
        }
        if (g_instructionDib) {
            DeleteObject(g_instructionDib);
        }
        DeleteDC(g_instructionMemDC);
    }

    g_instructionMemDC = nullptr;
    g_instructionDib = nullptr;
    g_instructionOldBitmap = nullptr;
    g_instructionDibBits = nullptr;
    g_instructionSurfaceW = 0;
    g_instructionSurfaceH = 0;
}

static bool EnsureInstructionSurface(int w, int h) {
    if (w <= 0 || h <= 0) return false;
    if (g_instructionMemDC &&
        g_instructionSurfaceW == w &&
        g_instructionSurfaceH == h) {
        return true;
    }

    DestroyInstructionSurface();

    HDC screen = GetDC(nullptr);
    g_instructionMemDC = CreateCompatibleDC(screen);
    ReleaseDC(nullptr, screen);
    if (!g_instructionMemDC) return false;

    BITMAPV5HEADER bh{};
    bh.bV5Size = sizeof(BITMAPV5HEADER);
    bh.bV5Width = w;
    bh.bV5Height = -h;
    bh.bV5Planes = 1;
    bh.bV5BitCount = 32;
    bh.bV5Compression = BI_BITFIELDS;
    bh.bV5RedMask = 0x00FF0000;
    bh.bV5GreenMask = 0x0000FF00;
    bh.bV5BlueMask = 0x000000FF;
    bh.bV5AlphaMask = 0xFF000000;
    bh.bV5CSType = LCS_sRGB;

    g_instructionDib = CreateDIBSection(
        g_instructionMemDC,
        reinterpret_cast<BITMAPINFO*>(&bh),
        DIB_RGB_COLORS,
        &g_instructionDibBits,
        nullptr,
        0
    );

    if (!g_instructionDib || !g_instructionDibBits) {
        DestroyInstructionSurface();
        return false;
    }

    g_instructionOldBitmap =
        SelectObject(g_instructionMemDC, g_instructionDib);
    g_instructionSurfaceW = w;
    g_instructionSurfaceH = h;
    return true;
}

static HRESULT RecreateTextFormats() {
    SafeRelease(g_textFormat);
    SafeRelease(g_smallFormat);

    float base = 15.0f * g_scale;
    float small = 12.0f * g_scale;

    HRESULT hr = g_dwFactory->CreateTextFormat(
        L"Microsoft YaHei UI",
        nullptr,
        DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        base,
        L"zh-cn",
        &g_textFormat
    );
    if (FAILED(hr)) return hr;

    hr = g_dwFactory->CreateTextFormat(
        L"Microsoft YaHei UI",
        nullptr,
        DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        small,
        L"zh-cn",
        &g_smallFormat
    );
    if (FAILED(hr)) return hr;

    g_textFormat->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);
    g_smallFormat->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);
    return S_OK;
}

static float MeasureText(const std::wstring& text, IDWriteTextFormat* fmt, float width, float maxHeight = 10000.0f) {
    if (!g_dwFactory || !fmt || width <= 1.0f) return 0.0f;

    IDWriteTextLayout* layout = nullptr;
    HRESULT hr = g_dwFactory->CreateTextLayout(
        text.c_str(),
        static_cast<UINT32>(text.size()),
        fmt,
        width,
        maxHeight,
        &layout
    );
    if (FAILED(hr) || !layout) return 0.0f;

    DWRITE_TEXT_METRICS m{};
    layout->GetMetrics(&m);
    layout->Release();
    return std::ceil(m.height);
}

static void UpdateMonitorAndScale() {
    HWND anchor = g_pasteTarget && IsWindow(g_pasteTarget) ? g_pasteTarget : GetForegroundWindow();
    HMONITOR mon = MonitorFromWindow(anchor ? anchor : g_hwnd, MONITOR_DEFAULTTONEAREST);

    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    GetMonitorInfoW(mon, &mi);
    g_monitorRect = mi.rcMonitor;

    UINT dpi = 96;
    // Use the anchor application's DPI because the overlay may still be physically
    // located on another monitor before UpdateLayeredWindow moves it.
    if (anchor && IsWindow(anchor)) {
        UINT d = GetDpiForWindow(anchor);
        if (d) dpi = d;
    } else if (g_hwnd) {
        UINT d = GetDpiForWindow(g_hwnd);
        if (d) dpi = d;
    }
    float newScale = static_cast<float>(dpi) / 96.0f;
    if (newScale < 0.75f) newScale = 0.75f;
    if (newScale > 3.0f) newScale = 3.0f;

    const bool scaleChanged =
        std::fabs(newScale - g_scale) > 0.001f;

    g_scale = newScale;

    if (scaleChanged ||
        !g_textFormat ||
        !g_smallFormat) {
        RecreateTextFormats();
    }
}

static float SlotCardHeight(const ClipSlot& slot, float cardWidth) {
    const float pad = 12.0f * g_scale;
    const float labelH = 20.0f * g_scale;
    const float contentW = std::max(20.0f, cardWidth - pad * 2.0f);

    if (slot.previewKind == PreviewKind::Image && slot.previewImage.valid()) {
        const float maxImgH = 220.0f * g_scale;
        const float scaleW = contentW / static_cast<float>(slot.previewImage.width);
        const float imgH = static_cast<float>(slot.previewImage.height) * scaleW;
        return pad + labelH + 6.0f * g_scale + std::min(maxImgH, std::max(60.0f * g_scale, imgH)) + pad;
    }

    std::wstring t = slot.previewText;
    if (t.size() > 3500) t.resize(3500);
    float textH = MeasureText(t, g_textFormat, contentW, 180.0f * g_scale);
    textH = std::clamp(textH, 24.0f * g_scale, 180.0f * g_scale);
    return pad + labelH + 5.0f * g_scale + textH + pad;
}

static void BuildLayoutAndGeometry() {
    UpdateMonitorAndScale();

    const int monitorW = g_monitorRect.right - g_monitorRect.left;
    const int monitorH = g_monitorRect.bottom - g_monitorRect.top;
    const int edge = 5;

    g_windowW = std::max(
        220,
        static_cast<int>(std::lround(monitorW * 0.20))
    );

    const float gap = 7.0f * g_scale;
    const float cardW = static_cast<float>(g_windowW);

    float y = 0.0f;
    g_layout.clear();

    for (int i = 0; i < static_cast<int>(g_slots.size()); ++i) {
        const float h = SlotCardHeight(g_slots[i], cardW);
        g_layout.push_back({ y, h, i });
        y += h + gap;
    }

    if (!g_layout.empty()) {
        y -= gap;
    }

    g_totalHeight = std::max(1.0f, y);

    const int maxH = std::max(100, monitorH - edge * 2);
    g_windowH = std::min(
        maxH,
        std::max(1, static_cast<int>(std::ceil(g_totalHeight)))
    );

    g_maxScroll = std::max(
        0.0f,
        g_totalHeight - static_cast<float>(g_windowH)
    );
    g_scrollY = std::clamp(g_scrollY, 0.0f, g_maxScroll);

    g_windowTargetX = g_monitorRect.right - g_windowW - edge;
    g_windowTargetY = g_monitorRect.top + edge;
}

static void BuildInstructionGeometry() {
    const int monitorW = g_monitorRect.right - g_monitorRect.left;
    const int edge = 5;

    const int minW = static_cast<int>(std::lround(330.0f * g_scale));
    const int maxW = static_cast<int>(std::lround(520.0f * g_scale));
    const int preferredW =
        static_cast<int>(std::lround(monitorW * 0.26));

    g_instructionW = std::clamp(preferredW, minW, maxW);
    g_instructionW = std::min(
        g_instructionW,
        std::max(180, monitorW - edge * 2)
    );

    const float pad = 12.0f * g_scale;
    const float textW = std::max(
        80.0f,
        static_cast<float>(g_instructionW) - pad * 2.0f
    );
    const float textH = MeasureText(
        kInstructions,
        g_textFormat,
        textW
    );

    const float statusGap = 8.0f * g_scale;
    const float statusH = 22.0f * g_scale;

    g_instructionH = std::max(
        static_cast<int>(std::ceil(
            pad + textH + statusGap + statusH + pad
        )),
        static_cast<int>(std::lround(150.0f * g_scale))
    );

    g_instructionTargetX = g_monitorRect.left + edge;
    g_instructionTargetY = g_monitorRect.top + edge;
}

static float SmoothStep01(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

static void ApplyAnimatedPanelPositions() {
    const float eased = SmoothStep01(g_panelSlide);
    const int overshoot = std::max(
        24,
        static_cast<int>(std::lround(36.0f * g_scale))
    );

    const int instructionOffscreenX =
        g_monitorRect.left - g_instructionW - overshoot;
    const int slotOffscreenX =
        g_monitorRect.right + overshoot;

    g_instructionX = static_cast<int>(std::lround(
        instructionOffscreenX +
        (g_instructionTargetX - instructionOffscreenX) * eased
    ));
    g_instructionY = g_instructionTargetY;

    g_windowX = static_cast<int>(std::lround(
        slotOffscreenX +
        (g_windowTargetX - slotOffscreenX) * eased
    ));
    g_windowY = g_windowTargetY;
}

static void EnsureSlotHoverStorage() {
    if (g_slotHoverAmount.size() != g_slots.size()) {
        g_slotHoverAmount.assign(g_slots.size(), 0.0f);
    }
}

static void DrawTextBlock(
    ID2D1RenderTarget* rt,
    IDWriteTextFormat* fmt,
    const std::wstring& text,
    const D2D1_RECT_F& rc,
    ID2D1Brush* brush
) {
    if (text.empty()) return;
    rt->DrawTextW(
        text.c_str(),
        static_cast<UINT32>(text.size()),
        fmt,
        rc,
        brush,
        D2D1_DRAW_TEXT_OPTIONS_CLIP,
        DWRITE_MEASURING_MODE_NATURAL
    );
}

static void RenderInstructions() {
    if (!g_panelVisible || !g_instructionHwnd || !g_dcTarget) return;

    BuildInstructionGeometry();
    ApplyAnimatedPanelPositions();

    if (!EnsureInstructionSurface(g_instructionW, g_instructionH)) return;

    RECT rc{ 0, 0, g_instructionW, g_instructionH };
    if (FAILED(g_dcTarget->BindDC(g_instructionMemDC, &rc))) return;

    g_dcTarget->SetDpi(96.0f, 96.0f);
    g_dcTarget->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    g_dcTarget->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);

    const float a = std::clamp(g_instructionAlpha, 0.20f, 0.90f);

    ID2D1SolidColorBrush* background = nullptr;
    ID2D1SolidColorBrush* borderBrush = nullptr;
    ID2D1SolidColorBrush* textBrush = nullptr;
    ID2D1SolidColorBrush* statusBrush = nullptr;

    g_dcTarget->CreateSolidColorBrush(
        D2D1::ColorF(0.0f, 0.0f, 0.0f, a),
        &background
    );
    g_dcTarget->CreateSolidColorBrush(
        D2D1::ColorF(0.15f, 0.55f, 1.0f, a),
        &borderBrush
    );
    g_dcTarget->CreateSolidColorBrush(
        D2D1::ColorF(1.0f, 1.0f, 1.0f, a),
        &textBrush
    );
    g_dcTarget->CreateSolidColorBrush(
        D2D1::ColorF(0.85f, 0.92f, 1.0f, a),
        &statusBrush
    );

    g_dcTarget->BeginDraw();
    g_dcTarget->Clear(D2D1::ColorF(0, 0, 0, 0));

    const float radius = 11.0f * g_scale;
    const float border = 2.5f * g_scale;
    const float pad = 12.0f * g_scale;

    D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(
        D2D1::RectF(
            0.5f * border,
            0.5f * border,
            static_cast<float>(g_instructionW) - 0.5f * border,
            static_cast<float>(g_instructionH) - 0.5f * border
        ),
        radius,
        radius
    );

    g_dcTarget->FillRoundedRectangle(rr, background);
    g_dcTarget->DrawRoundedRectangle(rr, borderBrush, border);

    const float textW =
        static_cast<float>(g_instructionW) - pad * 2.0f;
    const float textH = MeasureText(
        kInstructions,
        g_textFormat,
        textW
    );

    D2D1_RECT_F tr = D2D1::RectF(
        pad,
        pad,
        static_cast<float>(g_instructionW) - pad,
        pad + textH
    );
    DrawTextBlock(
        g_dcTarget,
        g_textFormat,
        kInstructions,
        tr,
        textBrush
    );

    if (!g_status.empty()) {
        D2D1_RECT_F sr = D2D1::RectF(
            pad,
            pad + textH + 8.0f * g_scale,
            static_cast<float>(g_instructionW) - pad,
            static_cast<float>(g_instructionH) - pad
        );
        DrawTextBlock(
            g_dcTarget,
            g_smallFormat,
            g_status,
            sr,
            statusBrush
        );
    }

    const HRESULT endHr = g_dcTarget->EndDraw();

    SafeRelease(background);
    SafeRelease(borderBrush);
    SafeRelease(textBrush);
    SafeRelease(statusBrush);

    if (FAILED(endHr)) return;

    POINT dst{ g_instructionX, g_instructionY };
    POINT src{ 0, 0 };
    SIZE size{ g_instructionW, g_instructionH };

    BLENDFUNCTION blend{};
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat = AC_SRC_ALPHA;

    HDC screen = GetDC(nullptr);
    UpdateLayeredWindow(
        g_instructionHwnd,
        screen,
        &dst,
        &size,
        g_instructionMemDC,
        &src,
        0,
        &blend,
        ULW_ALPHA
    );
    ReleaseDC(nullptr, screen);
}

static void RenderPanel() {
    if (!g_panelVisible || !g_hwnd || !g_dcTarget) return;

    BuildLayoutAndGeometry();
    ApplyAnimatedPanelPositions();
    EnsureSlotHoverStorage();

    if (!EnsureSurface(g_windowW, g_windowH)) return;

    RECT rc{ 0, 0, g_windowW, g_windowH };
    if (FAILED(g_dcTarget->BindDC(g_memDC, &rc))) return;

    g_dcTarget->SetDpi(96.0f, 96.0f);
    g_dcTarget->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    g_dcTarget->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);

    g_dcTarget->BeginDraw();
    g_dcTarget->Clear(D2D1::ColorF(0, 0, 0, 0));

    // Layered windows treat fully transparent pixels as mouse-transparent.
    // Paint the entire right-side window with an almost invisible non-zero
    // alpha so the gaps between slot cards still receive wheel/click input.
    ID2D1SolidColorBrush* hitSurfaceBrush = nullptr;
    g_dcTarget->CreateSolidColorBrush(
        D2D1::ColorF(
            0.0f,
            0.0f,
            0.0f,
            1.0f / 255.0f
        ),
        &hitSurfaceBrush
    );
    if (hitSurfaceBrush) {
        g_dcTarget->FillRectangle(
            D2D1::RectF(
                0.0f,
                0.0f,
                static_cast<float>(g_windowW),
                static_cast<float>(g_windowH)
            ),
            hitSurfaceBrush
        );
    }

    const float radius = 11.0f * g_scale;
    const float border = 3.0f * g_scale;
    const float pad = 12.0f * g_scale;
    const float labelH = 20.0f * g_scale;

    for (const auto& item : g_layout) {
        const float top = item.y - g_scrollY;
        const float bottom = top + item.h;

        if (bottom < 0.0f || top > g_windowH) continue;
        if (item.slotIndex < 0 ||
            item.slotIndex >= static_cast<int>(g_slots.size())) {
            continue;
        }

        const int slotIndex = item.slotIndex;
        const bool selected = slotIndex == g_selected;

        float hoverAmount = 0.0f;
        if (slotIndex >= 0 &&
            slotIndex < static_cast<int>(g_slotHoverAmount.size())) {
            hoverAmount = std::clamp(
                g_slotHoverAmount[slotIndex],
                0.0f,
                1.0f
            );
        }

        // Hover no longer changes slot opacity.
        // Selected slot stays at 90%, other slots stay at 60%.
        const float alpha = selected ? 0.90f : 0.60f;

        ID2D1SolidColorBrush* background = nullptr;
        ID2D1SolidColorBrush* borderBrush = nullptr;
        ID2D1SolidColorBrush* textBrush = nullptr;
        ID2D1SolidColorBrush* labelBrush = nullptr;
        ID2D1SolidColorBrush* scrollbarBrush = nullptr;

        g_dcTarget->CreateSolidColorBrush(
            D2D1::ColorF(0.0f, 0.0f, 0.0f, alpha),
            &background
        );

        if (selected) {
            // Current slot always keeps its green border.
            g_dcTarget->CreateSolidColorBrush(
                D2D1::ColorF(
                    0.20f,
                    0.95f,
                    0.38f,
                    alpha
                ),
                &borderBrush
            );
        } else {
            // Other slots smoothly transition from white to the same blue
            // used by the left instruction panel while hovered.
            const float borderR =
                1.00f + (0.15f - 1.00f) * hoverAmount;
            const float borderG =
                1.00f + (0.55f - 1.00f) * hoverAmount;
            const float borderB = 1.00f;

            g_dcTarget->CreateSolidColorBrush(
                D2D1::ColorF(
                    borderR,
                    borderG,
                    borderB,
                    alpha
                ),
                &borderBrush
            );
        }

        g_dcTarget->CreateSolidColorBrush(
            D2D1::ColorF(1.0f, 1.0f, 1.0f, alpha),
            &textBrush
        );
        g_dcTarget->CreateSolidColorBrush(
            D2D1::ColorF(
                selected ? 0.88f : 0.86f,
                selected ? 1.00f : 0.86f,
                selected ? 0.90f : 0.86f,
                alpha
            ),
            &labelBrush
        );
        g_dcTarget->CreateSolidColorBrush(
            D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.55f),
            &scrollbarBrush
        );

        D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(
            D2D1::RectF(
                0.5f * border,
                top + 0.5f * border,
                static_cast<float>(g_windowW) - 0.5f * border,
                bottom - 0.5f * border
            ),
            radius,
            radius
        );

        g_dcTarget->FillRoundedRectangle(rr, background);
        g_dcTarget->DrawRoundedRectangle(
            rr,
            borderBrush,
            border
        );

        const ClipSlot& slot = g_slots[slotIndex];

        std::wstring label =
            L"槽位 " +
            std::to_wstring(slotIndex + 1) +
            L" · " +
            slot.typeLabel;

        if (selected) {
            label = L"▶ " + label;
        }

        D2D1_RECT_F lr = D2D1::RectF(
            pad,
            top + pad,
            static_cast<float>(g_windowW) - pad,
            top + pad + labelH
        );

        DrawTextBlock(
            g_dcTarget,
            g_smallFormat,
            label,
            lr,
            labelBrush
        );

        const float contentTop =
            top + pad + labelH + 5.0f * g_scale;
        const float contentBottom = bottom - pad;
        const float contentW = std::max(
            1.0f,
            static_cast<float>(g_windowW) - pad * 2.0f
        );

        if (slot.previewKind == PreviewKind::Image &&
            slot.previewImage.valid()) {
            ID2D1Bitmap* bmp = nullptr;

            D2D1_BITMAP_PROPERTIES props{};
            props.pixelFormat = D2D1::PixelFormat(
                DXGI_FORMAT_B8G8R8A8_UNORM,
                D2D1_ALPHA_MODE_PREMULTIPLIED
            );
            props.dpiX = 96.0f;
            props.dpiY = 96.0f;

            const HRESULT hr = g_dcTarget->CreateBitmap(
                D2D1::SizeU(
                    slot.previewImage.width,
                    slot.previewImage.height
                ),
                slot.previewImage.bgra.data(),
                static_cast<UINT32>(
                    slot.previewImage.width * 4
                ),
                props,
                &bmp
            );

            if (SUCCEEDED(hr) && bmp) {
                const float srcAspect =
                    static_cast<float>(slot.previewImage.width) /
                    slot.previewImage.height;

                const float boxH =
                    std::max(1.0f, contentBottom - contentTop);

                float drawW = contentW;
                float drawH = drawW / srcAspect;

                if (drawH > boxH) {
                    drawH = boxH;
                    drawW = drawH * srcAspect;
                }

                const float x =
                    pad + (contentW - drawW) * 0.5f;

                D2D1_RECT_F dst = D2D1::RectF(
                    x,
                    contentTop,
                    x + drawW,
                    contentTop + drawH
                );

                g_dcTarget->DrawBitmap(
                    bmp,
                    dst,
                    alpha,
                    D2D1_BITMAP_INTERPOLATION_MODE_LINEAR
                );

                bmp->Release();
            }
        } else {
            std::wstring t = slot.previewText;
            if (t.size() > 3500) {
                t.resize(3500);
                t += L"…";
            }

            D2D1_RECT_F cr = D2D1::RectF(
                pad,
                contentTop,
                static_cast<float>(g_windowW) - pad,
                contentBottom
            );

            DrawTextBlock(
                g_dcTarget,
                g_textFormat,
                t,
                cr,
                textBrush
            );
        }

        SafeRelease(background);
        SafeRelease(borderBrush);
        SafeRelease(textBrush);
        SafeRelease(labelBrush);
        SafeRelease(scrollbarBrush);
    }

    if (g_maxScroll > 0.0f) {
        ID2D1SolidColorBrush* scrollbarBrush = nullptr;
        g_dcTarget->CreateSolidColorBrush(
            D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.55f),
            &scrollbarBrush
        );

        const float trackMargin = 4.0f * g_scale;
        const float trackW = 4.0f * g_scale;
        const float trackH =
            static_cast<float>(g_windowH) -
            2.0f * trackMargin;

        const float thumbH = std::max(
            28.0f * g_scale,
            trackH *
            (static_cast<float>(g_windowH) / g_totalHeight)
        );

        const float thumbTravel =
            std::max(1.0f, trackH - thumbH);

        const float thumbY =
            trackMargin +
            (g_scrollY / g_maxScroll) * thumbTravel;

        D2D1_ROUNDED_RECT sb = D2D1::RoundedRect(
            D2D1::RectF(
                static_cast<float>(g_windowW) -
                    trackMargin -
                    trackW,
                thumbY,
                static_cast<float>(g_windowW) -
                    trackMargin,
                thumbY + thumbH
            ),
            trackW * 0.5f,
            trackW * 0.5f
        );

        g_dcTarget->FillRoundedRectangle(
            sb,
            scrollbarBrush
        );
        SafeRelease(scrollbarBrush);
    }

    const HRESULT endHr = g_dcTarget->EndDraw();
    SafeRelease(hitSurfaceBrush);
    if (FAILED(endHr)) return;

    POINT dst{ g_windowX, g_windowY };
    POINT src{ 0, 0 };
    SIZE size{ g_windowW, g_windowH };

    BLENDFUNCTION blend{};
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat = AC_SRC_ALPHA;

    HDC screen = GetDC(nullptr);
    UpdateLayeredWindow(
        g_hwnd,
        screen,
        &dst,
        &size,
        g_memDC,
        &src,
        0,
        &blend,
        ULW_ALPHA
    );
    ReleaseDC(nullptr, screen);
}

static void SetStatus(const std::wstring& s) {
    g_status = s;
    KillTimer(g_hwnd, TIMER_STATUS);
    SetTimer(g_hwnd, TIMER_STATUS, 1600, nullptr);

    if (g_panelVisible) {
        RenderPanel();
        RenderInstructions();
    } else if (!s.empty()) {
        MessageBeep(MB_ICONINFORMATION);
    }
}

static bool HasSelectedSlot() {
    return g_selected >= 0 && g_selected < static_cast<int>(g_slots.size());
}

static void EnsureSelectionVisible() {
    if (!HasSelectedSlot()) return;

    for (const auto& item : g_layout) {
        if (item.slotIndex != g_selected) continue;

        float top = item.y;
        float bottom = item.y + item.h;
        if (top < g_scrollY) {
            g_scrollY = top;
        } else if (bottom > g_scrollY + g_windowH) {
            g_scrollY = bottom - g_windowH;
        }
        g_scrollY = std::clamp(g_scrollY, 0.0f, g_maxScroll);
        return;
    }
}

static void CycleSelection() {
    if (g_slots.empty()) {
        g_selected = -1;
        SetStatus(L"当前没有任何槽位");
        return;
    }
    if (g_selected < 0 || g_selected >= static_cast<int>(g_slots.size())) g_selected = 0;
    else g_selected = (g_selected + 1) % static_cast<int>(g_slots.size());

    BuildLayoutAndGeometry();
    EnsureSelectionVisible();
    RenderPanel();
}

static bool ClipboardOperationBusy() {
    return
        g_pastePhase != PastePhase::None ||
        g_captureSelectionPhase != CaptureSelectionPhase::None;
}

static void AddSlotToFront(ClipSlot&& slot) {
    g_slots.insert(g_slots.begin(), std::move(slot));
    g_slotHoverAmount.insert(g_slotHoverAmount.begin(), 0.0f);
    g_selected = 0;
    g_scrollY = 0.0f;

    if (g_panelVisible) {
        RenderPanel();
        RenderInstructions();
    }
}

static void AddClipboardSlot() {
    if (ClipboardOperationBusy()) {
        SetStatus(L"上一项剪贴板操作尚未结束");
        return;
    }

    ClipSlot slot;
    std::wstring err;

    if (!CaptureClipboard(slot, err)) {
        SetStatus(err);
        return;
    }

    AddSlotToFront(std::move(slot));

    if (g_panelVisible) {
        SetStatus(L"已添加为槽位 1");
    } else {
        MessageBeep(MB_OK);
    }
}

static void CopySelectedToClipboard() {
    if (ClipboardOperationBusy()) {
        SetStatus(L"上一项剪贴板操作尚未结束");
        return;
    }

    if (!HasSelectedSlot()) {
        SetStatus(L"当前没有任何槽位");
        return;
    }

    std::wstring err;
    if (!PutSlotOnClipboard(g_slots[g_selected], err)) {
        SetStatus(err);
        return;
    }

    SetStatus(
        L"已复制槽位 " +
        std::to_wstring(g_selected + 1) +
        L" 到系统剪贴板"
    );
}

static bool DeleteSlotAt(int index) {
    if (index < 0 || index >= static_cast<int>(g_slots.size())) {
        return false;
    }

    g_slots.erase(g_slots.begin() + index);
    if (index >= 0 &&
        index < static_cast<int>(g_slotHoverAmount.size())) {
        g_slotHoverAmount.erase(
            g_slotHoverAmount.begin() + index
        );
    }
    g_hoveredSlot = -1;

    if (g_slots.empty()) {
        g_selected = -1;
    } else if (g_selected > index) {
        --g_selected;
    } else if (g_selected == index) {
        if (index >= static_cast<int>(g_slots.size())) {
            g_selected = static_cast<int>(g_slots.size()) - 1;
        } else {
            g_selected = index;
        }
    }

    g_scrollY = std::max(0.0f, g_scrollY);

    if (g_panelVisible) {
        RenderPanel();
        RenderInstructions();
    }

    return true;
}

static void DeleteSelected() {
    if (ClipboardOperationBusy()) {
        SetStatus(L"上一项剪贴板操作尚未结束");
        return;
    }

    if (!HasSelectedSlot()) {
        SetStatus(L"当前没有任何槽位");
        return;
    }

    DeleteSlotAt(g_selected);
    SetStatus(L"已删除当前槽位");
}

static void DeleteLatest() {
    if (ClipboardOperationBusy()) {
        SetStatus(L"上一项剪贴板操作尚未结束");
        return;
    }

    if (g_slots.empty()) {
        SetStatus(L"当前没有任何槽位");
        return;
    }

    DeleteSlotAt(0);

    if (g_panelVisible) {
        SetStatus(L"已删除槽位 1");
    }
}

static bool SendCtrlKey(WORD vk) {
    INPUT in[4]{};

    in[0].type = INPUT_KEYBOARD;
    in[0].ki.wVk = VK_CONTROL;

    in[1].type = INPUT_KEYBOARD;
    in[1].ki.wVk = vk;

    in[2].type = INPUT_KEYBOARD;
    in[2].ki.wVk = vk;
    in[2].ki.dwFlags = KEYEVENTF_KEYUP;

    in[3].type = INPUT_KEYBOARD;
    in[3].ki.wVk = VK_CONTROL;
    in[3].ki.dwFlags = KEYEVENTF_KEYUP;

    const UINT sent = SendInput(4, in, sizeof(INPUT));
    if (sent == 4) {
        return true;
    }

    INPUT up[2]{};
    up[0].type = INPUT_KEYBOARD;
    up[0].ki.wVk = vk;
    up[0].ki.dwFlags = KEYEVENTF_KEYUP;
    up[1].type = INPUT_KEYBOARD;
    up[1].ki.wVk = VK_CONTROL;
    up[1].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(2, up, sizeof(INPUT));

    return false;
}

static bool SendCtrlV() {
    return SendCtrlKey('V');
}

static bool SendCtrlC() {
    return SendCtrlKey('C');
}

static bool AnyKeyboardModifierDown() {
    return
        (GetAsyncKeyState(VK_MENU) & 0x8000) != 0 ||
        (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0 ||
        (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
}

static bool SendPasteFromRegisteredHotkey(
    WORD triggerVk,
    bool restoreCtrlModifier
) {
    INPUT in[10]{};
    UINT count = 0;

    auto addKey = [&](WORD vk, bool keyUp) {
        in[count].type = INPUT_KEYBOARD;
        in[count].ki.wVk = vk;
        in[count].ki.dwFlags =
            keyUp ? KEYEVENTF_KEYUP : 0;
        ++count;
    };

    // The registered hotkey fires while these keys are logically down.
    // Temporarily release them so the injected Ctrl+V is not interpreted
    // as Alt+Ctrl+V / Ctrl+Alt+1, then restore the held modifiers.
    if (triggerVk != 0) {
        addKey(triggerVk, true);
    }

    if (restoreCtrlModifier) {
        addKey(VK_CONTROL, true);
    }

    addKey(VK_MENU, true);

    // Actual paste chord.
    addKey(VK_CONTROL, false);
    addKey('V', false);
    addKey('V', true);
    addKey(VK_CONTROL, true);

    // Recreate the modifier-down state expected while the user is still
    // physically holding Alt (and Ctrl for Ctrl+Alt+1). The user's real
    // key-up later clears these states normally.
    if (restoreCtrlModifier) {
        addKey(VK_CONTROL, false);
    }

    addKey(VK_MENU, false);

    const UINT sent = SendInput(
        count,
        in,
        sizeof(INPUT)
    );

    if (sent == count) {
        return true;
    }

    // Best-effort modifier restoration after a partial SendInput.
    INPUT restore[2]{};
    UINT restoreCount = 0;

    if (restoreCtrlModifier) {
        restore[restoreCount].type = INPUT_KEYBOARD;
        restore[restoreCount].ki.wVk = VK_CONTROL;
        ++restoreCount;
    }

    restore[restoreCount].type = INPUT_KEYBOARD;
    restore[restoreCount].ki.wVk = VK_MENU;
    ++restoreCount;

    SendInput(
        restoreCount,
        restore,
        sizeof(INPUT)
    );

    return false;
}

static bool EnsureForegroundTarget(HWND target) {
    if (!target || !IsWindow(target)) {
        return false;
    }

    if (GetForegroundWindow() == target) {
        return true;
    }

    SetForegroundWindow(target);
    return GetForegroundWindow() == target;
}

static void FinishPasteState() {
    g_pastePhase = PastePhase::None;
    g_pasteRestoreTicks = 0;
    g_pasteRestoreDelayTicks = 25;
    g_pasteTempSequence = 0;
    g_pasteBackup = ClipboardBackup{};
    KillTimer(g_hwnd, TIMER_PASTE);
}

static void StartPasteSlot(
    int slotIndex,
    bool deleteAfterCopy,
    WORD triggerVk,
    bool restoreCtrlModifier
) {
    if (g_captureSelectionPhase !=
        CaptureSelectionPhase::None) {
        SetStatus(L"上一项剪贴板操作尚未结束");
        return;
    }

    if (slotIndex < 0 ||
        slotIndex >= static_cast<int>(g_slots.size())) {
        SetStatus(L"当前没有可粘贴的槽位");
        return;
    }

    // A running RestoreClipboard phase means the system clipboard currently
    // contains RamClip's temporary paste data. Do NOT back it up again.
    // Keep the original user clipboard snapshot and simply postpone restore.
    bool continuingPasteBurst =
        g_pastePhase == PastePhase::RestoreClipboard &&
        g_pasteBackup.valid;

    if (continuingPasteBurst &&
        g_pasteTempSequence != 0 &&
        GetClipboardSequenceNumber() !=
            g_pasteTempSequence) {
        // Someone changed the clipboard during the debounce window.
        // Preserve that newer clipboard instead of restoring an older one.
        FinishPasteState();
        continuingPasteBurst = false;
    }

    UpdatePasteTargetFromForeground();

    if (!continuingPasteBurst) {
        std::wstring backupErr;
        ClipboardBackup backup;

        if (!SaveClipboardBackup(backup, backupErr)) {
            SetStatus(
                L"为保护系统剪贴板，已取消粘贴：" +
                backupErr
            );
            return;
        }

        g_pasteBackup = std::move(backup);
    }

    // Copy before a paste-and-delete operation can erase the slot.
    ClipSlot pasteSlot = g_slots[slotIndex];

    if (!EnsureForegroundTarget(g_pasteTarget)) {
        if (!continuingPasteBurst) {
            FinishPasteState();
        }

        SetStatus(
            L"无法切换到原粘贴窗口，已取消本次粘贴"
        );
        return;
    }

    std::wstring writeErr;
    if (!PutSlotOnClipboard(pasteSlot, writeErr)) {
        std::wstring restoreErr;
        const bool restored =
            RestoreClipboardBackup(
                g_pasteBackup,
                restoreErr
            );

        FinishPasteState();

        if (restored) {
            SetStatus(
                L"粘贴前写入临时剪贴板失败：" +
                writeErr
            );
        } else {
            SetStatus(
                L"粘贴前写入临时剪贴板失败：" +
                writeErr +
                L"；恢复系统剪贴板也失败：" +
                restoreErr
            );
        }
        return;
    }

    g_pasteTempSequence =
        GetClipboardSequenceNumber();

    if (!SendPasteFromRegisteredHotkey(
            triggerVk,
            restoreCtrlModifier
        )) {
        std::wstring restoreErr;
        const bool restored =
            RestoreClipboardBackup(
                g_pasteBackup,
                restoreErr
            );

        FinishPasteState();

        if (restored) {
            SetStatus(
                L"无法发送粘贴按键；目标程序可能以更高权限运行"
            );
        } else {
            SetStatus(
                L"无法发送粘贴按键，且恢复系统剪贴板失败：" +
                restoreErr
            );
        }
        return;
    }

    // Paste-and-delete now commits after the paste input was injected.
    // This allows repeated Ctrl+Alt+1 presses to advance through slots.
    if (deleteAfterCopy) {
        DeleteSlotAt(slotIndex);
    }

    // Debounce clipboard restoration: every successful paste restarts the
    // quiet-period countdown, so rapid repeated pastes share one backup.
    g_pasteRestoreDelayTicks =
        FindFormat(pasteSlot, CF_HDROP)
            ? 40   // ~800 ms for file-drop consumers
            : 25;  // ~500 ms for text/images

    g_pasteRestoreTicks = 0;
    g_pastePhase = PastePhase::RestoreClipboard;

    KillTimer(g_hwnd, TIMER_PASTE);
    SetTimer(g_hwnd, TIMER_PASTE, 20, nullptr);
}

static void StartPasteSelected(bool deleteAfterCopy) {
    if (!HasSelectedSlot()) {
        SetStatus(L"当前没有任何槽位");
        return;
    }

    StartPasteSlot(
        g_selected,
        deleteAfterCopy,
        deleteAfterCopy ? '1' : '5',
        deleteAfterCopy
    );
}

static void StartPasteLatest() {
    if (g_slots.empty()) {
        SetStatus(L"当前没有任何槽位");
        return;
    }

    StartPasteSlot(
        0,
        false,
        'V',
        false
    );
}

static void FinishCaptureSelectionState() {
    g_captureSelectionPhase = CaptureSelectionPhase::None;
    g_captureSelectionTicks = 0;
    g_captureSequenceBefore = 0;
    g_captureTarget = nullptr;
    g_captureBackup = ClipboardBackup{};
    KillTimer(g_hwnd, TIMER_CAPTURE_SELECTION);
}

static void StartCaptureSelectionToLatest() {
    if (g_pastePhase != PastePhase::None ||
        g_captureSelectionPhase != CaptureSelectionPhase::None) {
        SetStatus(L"上一项剪贴板操作尚未结束");
        return;
    }

    HWND fg = GetForegroundWindow();
    if (!fg || fg == g_hwnd || fg == g_instructionHwnd) {
        SetStatus(L"没有可复制选区的前台窗口");
        return;
    }

    std::wstring err;
    ClipboardBackup backup;

    if (!SaveClipboardBackup(backup, err)) {
        SetStatus(
            L"为保护系统剪贴板，已取消 Alt+C：" + err
        );
        return;
    }

    g_captureTarget = fg;
    g_captureBackup = std::move(backup);
    g_captureSelectionPhase = CaptureSelectionPhase::WaitModifiers;
    g_captureSelectionTicks = 0;

    KillTimer(g_hwnd, TIMER_CAPTURE_SELECTION);
    SetTimer(g_hwnd, TIMER_CAPTURE_SELECTION, 20, nullptr);
}

static int HitTestSlot(float clientY);
static void UnregisterPanelHotkeys();
static void HidePanel();
static void InstallPanelMouseHook();
static void RemovePanelMouseHook();

static int HitTestSlotAtScreenPoint(const POINT& cursor) {
    if (!g_panelVisible || g_panelSlide <= 0.001f) {
        return -1;
    }

    RECT panelRect{
        g_windowX,
        g_windowY,
        g_windowX + g_windowW,
        g_windowY + g_windowH
    };

    if (!PtInRect(&panelRect, cursor)) {
        return -1;
    }

    const float clientY =
        static_cast<float>(cursor.y - g_windowY);

    return HitTestSlot(clientY);
}

static bool AnimateToward(
    float& value,
    float target,
    float factor,
    float snap = 0.004f
) {
    const float old = value;
    const float delta = target - value;

    if (std::fabs(delta) <= snap) {
        value = target;
    } else {
        value += delta * factor;
    }

    return std::fabs(value - old) > 0.0005f;
}

static void UpdateUiAnimation() {
    if (!g_panelVisible) return;

    bool needInstructionRender = false;
    bool needPanelRender = false;

    const float slideTarget =
        g_panelOpenTarget ? 1.0f : 0.0f;

    if (AnimateToward(
            g_panelSlide,
            slideTarget,
            0.18f,
            0.005f
        )) {
        needInstructionRender = true;
        needPanelRender = true;
    }

    POINT cursor{};
    const bool haveCursor = !!GetCursorPos(&cursor);

    bool instructionHovered = false;
    if (g_panelOpenTarget && haveCursor) {
        RECT r{
            g_instructionX,
            g_instructionY,
            g_instructionX + g_instructionW,
            g_instructionY + g_instructionH
        };
        instructionHovered = !!PtInRect(&r, cursor);
    }

    g_instructionHovered = instructionHovered;

    const float instructionTarget =
        instructionHovered ? 0.20f : 0.90f;

    if (AnimateToward(
            g_instructionAlpha,
            instructionTarget,
            0.18f
        )) {
        needInstructionRender = true;
    }

    EnsureSlotHoverStorage();

    int hoveredSlot = -1;
    if (g_panelOpenTarget && haveCursor) {
        hoveredSlot = HitTestSlotAtScreenPoint(cursor);
    }

    g_hoveredSlot = hoveredSlot;

    for (int i = 0;
         i < static_cast<int>(g_slotHoverAmount.size());
         ++i) {
        const float target =
            (i == hoveredSlot) ? 1.0f : 0.0f;

        if (AnimateToward(
                g_slotHoverAmount[i],
                target,
                0.20f
            )) {
            needPanelRender = true;
        }
    }

    if (needPanelRender) {
        RenderPanel();
    }

    if (needInstructionRender) {
        RenderInstructions();
    }

    if (!g_panelOpenTarget &&
        g_panelSlide <= 0.001f) {
        g_panelSlide = 0.0f;
        g_panelVisible = false;

        UnregisterPanelHotkeys();

        ShowWindow(g_hwnd, SW_HIDE);
        if (g_instructionHwnd) {
            ShowWindow(g_instructionHwnd, SW_HIDE);
        }

        KillTimer(g_hwnd, TIMER_UI_ANIMATION);
        KillTimer(g_hwnd, TIMER_TOPMOST_WATCH);
    }
}

static void RegisterPanelHotkeys() {
    if (g_panelHotkeysRegistered) return;

    const bool okCycle = !!RegisterHotKey(
        g_hwnd,
        HK_CYCLE,
        MOD_ALT | MOD_NOREPEAT,
        '1'
    );
    const bool okCopy = !!RegisterHotKey(
        g_hwnd,
        HK_COPY_CURRENT,
        MOD_ALT | MOD_NOREPEAT,
        '3'
    );
    const bool okDelete = !!RegisterHotKey(
        g_hwnd,
        HK_DELETE,
        MOD_ALT | MOD_NOREPEAT,
        '4'
    );

    if (okCycle &&
        okCopy &&
        okDelete) {
        g_panelHotkeysRegistered = true;
        return;
    }

    UnregisterHotKey(g_hwnd, HK_CYCLE);
    UnregisterHotKey(g_hwnd, HK_COPY_CURRENT);
    UnregisterHotKey(g_hwnd, HK_DELETE);

    g_panelHotkeysRegistered = false;
    SetStatus(
        L"部分面板快捷键注册失败，可能与其他软件冲突"
    );
}

static void UnregisterPanelHotkeys() {
    if (!g_panelHotkeysRegistered) return;

    UnregisterHotKey(g_hwnd, HK_CYCLE);
    UnregisterHotKey(g_hwnd, HK_COPY_CURRENT);
    UnregisterHotKey(g_hwnd, HK_DELETE);

    g_panelHotkeysRegistered = false;
}

static void ShowPanel() {
    UpdatePasteTargetFromForeground();

    g_panelOpenTarget = true;

    // Every time the UI opens, slot 1 is the selected slot.
    g_selected = g_slots.empty() ? -1 : 0;
    g_scrollY = 0.0f;

    if (!g_panelVisible) {
        g_panelVisible = true;
        g_panelSlide = 0.0f;
        g_instructionAlpha = 0.90f;
        g_instructionHovered = false;
        g_hoveredSlot = -1;
        EnsureSlotHoverStorage();

        for (float& v : g_slotHoverAmount) {
            v = 0.0f;
        }

        ShowWindow(g_hwnd, SW_SHOWNOACTIVATE);
        ShowWindow(
            g_instructionHwnd,
            SW_SHOWNOACTIVATE
        );
    }

    // Reassert topmost every time the UI opens. This puts both overlays
    // back at the front of the current TOPMOST band without activating them.
    EnsurePanelsTopmost(true);

    // While visible, periodically repair TOPMOST only if another program or
    // a shell/display transition actually removed it.
    KillTimer(g_hwnd, TIMER_TOPMOST_WATCH);
    SetTimer(g_hwnd, TIMER_TOPMOST_WATCH, 1000, nullptr);

    RegisterPanelHotkeys();
    InstallPanelMouseHook();

    RenderPanel();
    RenderInstructions();

    KillTimer(g_hwnd, TIMER_UI_ANIMATION);
    SetTimer(g_hwnd, TIMER_UI_ANIMATION, 16, nullptr);
}

static void HidePanel() {
    if (!g_panelVisible) return;

    g_panelOpenTarget = false;
    g_instructionHovered = false;
    g_hoveredSlot = -1;

    // The visuals continue sliding out, but interactive panel-only input stops
    // immediately so one close action cannot accidentally trigger another.
    UnregisterPanelHotkeys();
    RemovePanelMouseHook();

    KillTimer(g_hwnd, TIMER_TOPMOST_WATCH);
    KillTimer(g_hwnd, TIMER_UI_ANIMATION);
    SetTimer(g_hwnd, TIMER_UI_ANIMATION, 16, nullptr);
}

static void TogglePanel() {
    if (g_panelVisible && g_panelOpenTarget) {
        HidePanel();
    } else {
        ShowPanel();
    }
}

static int HitTestSlot(float clientY) {
    float logicalY = clientY + g_scrollY;
    for (const auto& item : g_layout) {
        if (item.slotIndex < 0) continue;
        if (logicalY >= item.y && logicalY <= item.y + item.h) return item.slotIndex;
    }
    return -1;
}

static LRESULT CALLBACK PanelMouseHookProc(
    int code,
    WPARAM wParam,
    LPARAM lParam
) {
    if (code >= 0 &&
        g_panelVisible &&
        g_panelOpenTarget &&
        (wParam == WM_LBUTTONDOWN ||
         wParam == WM_RBUTTONDOWN ||
         wParam == WM_MBUTTONDOWN)) {

        const auto* mouse =
            reinterpret_cast<const MSLLHOOKSTRUCT*>(lParam);

        if (mouse) {
            const int slot =
                HitTestSlotAtScreenPoint(mouse->pt);

            // Clicking a real slot is handled by the right-side window.
            // Any other click closes the UI but is not swallowed.
            if (slot < 0 && g_hwnd) {
                PostMessageW(
                    g_hwnd,
                    WM_APP_OUTSIDE_CLICK,
                    0,
                    0
                );
            }
        }
    }

    return CallNextHookEx(
        g_panelMouseHook,
        code,
        wParam,
        lParam
    );
}

static void InstallPanelMouseHook() {
    if (g_panelMouseHook) return;

    g_panelMouseHook = SetWindowsHookExW(
        WH_MOUSE_LL,
        PanelMouseHookProc,
        GetModuleHandleW(nullptr),
        0
    );
}

static void RemovePanelMouseHook() {
    if (!g_panelMouseHook) return;

    UnhookWindowsHookEx(g_panelMouseHook);
    g_panelMouseHook = nullptr;
}

static LRESULT CALLBACK InstructionWndProc(
    HWND hwnd,
    UINT msg,
    WPARAM wParam,
    LPARAM lParam
) {
    switch (msg) {
    case WM_MOUSEACTIVATE:
        return MA_NOACTIVATE;
    case WM_NCHITTEST:
        return HTTRANSPARENT;
    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

static LRESULT CALLBACK WndProc(
    HWND hwnd,
    UINT msg,
    WPARAM wParam,
    LPARAM lParam
) {
    switch (msg) {
    case WM_CREATE:
        return 0;

    case WM_MOUSEACTIVATE:
        return MA_NOACTIVATE;

    case WM_NCHITTEST: {
        POINT pt{
            GET_X_LPARAM(lParam),
            GET_Y_LPARAM(lParam)
        };
        ScreenToClient(hwnd, &pt);

        if (pt.x < 0 ||
            pt.x >= g_windowW ||
            pt.y < 0 ||
            pt.y >= g_windowH) {
            return HTTRANSPARENT;
        }

        // The entire right-side clipboard window is one continuous hit area,
        // so wheel input works even in the gaps between slot cards.
        return HTCLIENT;
    }

    case WM_LBUTTONDOWN: {
        const int idx = HitTestSlot(
            static_cast<float>(GET_Y_LPARAM(lParam))
        );

        if (idx >= 0 &&
            idx < static_cast<int>(g_slots.size())) {
            g_selected = idx;
            RenderPanel();
        } else {
            // A click in a right-side gap is not a slot click.
            HidePanel();
        }
        return 0;
    }

    case WM_LBUTTONUP:
    case WM_LBUTTONDBLCLK:
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
    case WM_RBUTTONDBLCLK:
    case WM_MBUTTONDOWN:
    case WM_MBUTTONUP:
        return 0;

    case WM_MOUSEWHEEL: {
        if (g_maxScroll <= 0.0f) return 0;

        const int delta = GET_WHEEL_DELTA_WPARAM(wParam);
        g_scrollY -=
            static_cast<float>(delta) /
            WHEEL_DELTA *
            70.0f *
            g_scale;

        g_scrollY = std::clamp(
            g_scrollY,
            0.0f,
            g_maxScroll
        );

        RenderPanel();
        return 0;
    }

    case WM_DPICHANGED:
    case WM_DISPLAYCHANGE:
        if (g_panelVisible) {
            EnsurePanelsTopmost(true);
            RenderPanel();
            RenderInstructions();
        }
        return 0;

    case WM_APP_OUTSIDE_CLICK:
        if (g_panelVisible && g_panelOpenTarget) {
            HidePanel();
        }
        return 0;

    case WM_HOTKEY:
        switch (static_cast<int>(wParam)) {
        case HK_TOGGLE:
            TogglePanel();
            break;

        case HK_ADD:
            AddClipboardSlot();
            break;

        case HK_EXIT:
            DestroyWindow(hwnd);
            break;

        case HK_GLOBAL_CAPTURE:
            StartCaptureSelectionToLatest();
            break;

        case HK_GLOBAL_PASTE:
            StartPasteLatest();
            if (g_panelVisible && g_panelOpenTarget) {
                HidePanel();
            }
            break;

        case HK_GLOBAL_DELETE:
            DeleteLatest();
            break;

        case HK_CYCLE:
            if (g_panelVisible) {
                CycleSelection();
            }
            break;

        case HK_COPY_CURRENT:
            if (g_panelVisible && g_panelOpenTarget) {
                CopySelectedToClipboard();
                HidePanel();
            }
            break;

        case HK_DELETE:
            if (g_panelVisible) {
                DeleteSelected();
            }
            break;

        case HK_PASTE:
            StartPasteSelected(false);
            if (g_panelVisible && g_panelOpenTarget) {
                HidePanel();
            }
            break;

        case HK_PASTE_DELETE:
            StartPasteSelected(true);
            if (g_panelVisible && g_panelOpenTarget) {
                HidePanel();
            }
            break;
        }
        return 0;

    case WM_TIMER:
        if (wParam == TIMER_STATUS) {
            KillTimer(hwnd, TIMER_STATUS);
            g_status.clear();

            if (g_panelVisible) {
                RenderPanel();
                RenderInstructions();
            }
            return 0;
        }

        if (wParam == TIMER_UI_ANIMATION) {
            UpdateUiAnimation();
            return 0;
        }

        if (wParam == TIMER_TOPMOST_WATCH) {
            if (!g_panelVisible || !g_panelOpenTarget) {
                KillTimer(hwnd, TIMER_TOPMOST_WATCH);
                return 0;
            }

            // Normal watchdog pass: only repair actual demotion.
            EnsurePanelsTopmost(false);
            return 0;
        }

        if (wParam == TIMER_PASTE) {
            if (g_pastePhase !=
                PastePhase::RestoreClipboard) {
                KillTimer(hwnd, TIMER_PASTE);
                return 0;
            }

            ++g_pasteRestoreTicks;

            if (g_pasteRestoreTicks <
                g_pasteRestoreDelayTicks) {
                return 0;
            }

            if (g_pasteTempSequence != 0 &&
                GetClipboardSequenceNumber() !=
                    g_pasteTempSequence) {
                FinishPasteState();

                if (g_panelVisible) {
                    SetStatus(
                        L"系统剪贴板已被其他程序更新，保留新内容"
                    );
                }
                return 0;
            }

            std::wstring err;
            if (RestoreClipboardBackup(
                    g_pasteBackup,
                    err
                )) {
                FinishPasteState();

                if (g_panelVisible) {
                    SetStatus(L"已恢复系统剪贴板");
                }
                return 0;
            }

            // Keep retrying for about 3 seconds total. A new paste hotkey
            // during this period resets g_pasteRestoreTicks to zero.
            if (g_pasteRestoreTicks >= 150) {
                FinishPasteState();
                SetStatus(
                    L"粘贴完成，但恢复系统剪贴板失败：" +
                    err
                );
            }

            return 0;
        }

        if (wParam == TIMER_CAPTURE_SELECTION) {
            if (g_captureSelectionPhase ==
                CaptureSelectionPhase::None) {
                KillTimer(
                    hwnd,
                    TIMER_CAPTURE_SELECTION
                );
                return 0;
            }

            if (g_captureSelectionPhase ==
                CaptureSelectionPhase::WaitModifiers) {
                ++g_captureSelectionTicks;

                if (!AnyKeyboardModifierDown() ||
                    g_captureSelectionTicks > 100) {
                    if (!EnsureForegroundTarget(
                            g_captureTarget
                        )) {
                        FinishCaptureSelectionState();
                        SetStatus(
                            L"无法切换到原复制窗口，已取消 Alt+C"
                        );
                        return 0;
                    }

                    g_captureSequenceBefore =
                        GetClipboardSequenceNumber();

                    if (!SendCtrlC()) {
                        FinishCaptureSelectionState();
                        SetStatus(
                            L"无法发送 Ctrl+C；目标程序可能以更高权限运行"
                        );
                        return 0;
                    }

                    g_captureSelectionPhase =
                        CaptureSelectionPhase::WaitClipboard;
                    g_captureSelectionTicks = 0;
                }

                return 0;
            }

            if (g_captureSelectionPhase ==
                CaptureSelectionPhase::WaitClipboard) {
                ++g_captureSelectionTicks;

                const DWORD currentSequence =
                    GetClipboardSequenceNumber();

                const bool changed =
                    currentSequence !=
                    g_captureSequenceBefore;

                if (changed) {
                    ClipSlot slot;
                    std::wstring captureErr;

                    if (!CaptureClipboard(
                            slot,
                            captureErr
                        )) {
                        if (g_captureSelectionTicks < 50) {
                            return 0;
                        }

                        std::wstring restoreErr;
                        RestoreClipboardBackup(
                            g_captureBackup,
                            restoreErr
                        );

                        FinishCaptureSelectionState();

                        SetStatus(
                            L"无法读取当前选中内容：" +
                            captureErr
                        );
                        return 0;
                    }

                    std::wstring restoreErr;
                    const bool restored =
                        RestoreClipboardBackup(
                            g_captureBackup,
                            restoreErr
                        );

                    FinishCaptureSelectionState();
                    AddSlotToFront(std::move(slot));

                    if (!restored) {
                        SetStatus(
                            L"选中内容已加入槽位 1，但恢复系统剪贴板失败：" +
                            restoreErr
                        );
                    } else if (g_panelVisible) {
                        SetStatus(L"选中内容已加入槽位 1");
                    }

                    return 0;
                }

                if (g_captureSelectionTicks >= 50) {
                    std::wstring restoreErr;
                    const bool restored =
                        RestoreClipboardBackup(
                            g_captureBackup,
                            restoreErr
                        );

                    FinishCaptureSelectionState();

                    if (!restored) {
                        SetStatus(
                            L"未检测到可复制选区，且恢复系统剪贴板失败：" +
                            restoreErr
                        );
                    } else {
                        SetStatus(
                            L"未检测到可复制的选中内容"
                        );
                    }
                }

                return 0;
            }
        }
        break;

    case WM_DESTROY:
        UnregisterPanelHotkeys();
        RemovePanelMouseHook();

        UnregisterHotKey(hwnd, HK_TOGGLE);
        UnregisterHotKey(hwnd, HK_ADD);
        UnregisterHotKey(hwnd, HK_EXIT);
        UnregisterHotKey(hwnd, HK_GLOBAL_CAPTURE);
        UnregisterHotKey(hwnd, HK_GLOBAL_PASTE);
        UnregisterHotKey(hwnd, HK_GLOBAL_DELETE);
        UnregisterHotKey(hwnd, HK_PASTE);
        UnregisterHotKey(hwnd, HK_PASTE_DELETE);

        KillTimer(hwnd, TIMER_STATUS);
        KillTimer(hwnd, TIMER_PASTE);
        KillTimer(hwnd, TIMER_CAPTURE_SELECTION);
        KillTimer(hwnd, TIMER_UI_ANIMATION);
        KillTimer(hwnd, TIMER_TOPMOST_WATCH);

        if (g_instructionHwnd) {
            DestroyWindow(g_instructionHwnd);
            g_instructionHwnd = nullptr;
        }

        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(
        hwnd,
        msg,
        wParam,
        lParam
    );
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int) {
    SetProcessDpiAwarenessContext(
        DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
    );

    HRESULT hr = D2D1CreateFactory(
        D2D1_FACTORY_TYPE_SINGLE_THREADED,
        &g_d2dFactory
    );
    if (FAILED(hr)) {
        MessageBoxW(
            nullptr,
            L"Direct2D 初始化失败。",
            kAppTitle,
            MB_ICONERROR
        );
        return 1;
    }

    hr = DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_SHARED,
        __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(&g_dwFactory)
    );
    if (FAILED(hr)) {
        MessageBoxW(
            nullptr,
            L"DirectWrite 初始化失败。",
            kAppTitle,
            MB_ICONERROR
        );
        SafeRelease(g_d2dFactory);
        return 1;
    }

    D2D1_RENDER_TARGET_PROPERTIES props =
        D2D1::RenderTargetProperties(
            D2D1_RENDER_TARGET_TYPE_SOFTWARE,
            D2D1::PixelFormat(
                DXGI_FORMAT_B8G8R8A8_UNORM,
                D2D1_ALPHA_MODE_PREMULTIPLIED
            ),
            96.0f,
            96.0f,
            D2D1_RENDER_TARGET_USAGE_NONE,
            D2D1_FEATURE_LEVEL_DEFAULT
        );

    hr = g_d2dFactory->CreateDCRenderTarget(
        &props,
        &g_dcTarget
    );
    if (FAILED(hr)) {
        MessageBoxW(
            nullptr,
            L"Direct2D 渲染目标初始化失败。",
            kAppTitle,
            MB_ICONERROR
        );
        SafeRelease(g_dwFactory);
        SafeRelease(g_d2dFactory);
        return 1;
    }

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"RamClipOverlayWindow";
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);

    if (!RegisterClassExW(&wc)) {
        MessageBoxW(
            nullptr,
            L"窗口类注册失败。",
            kAppTitle,
            MB_ICONERROR
        );
        SafeRelease(g_dcTarget);
        SafeRelease(g_dwFactory);
        SafeRelease(g_d2dFactory);
        return 1;
    }

    WNDCLASSEXW instructionClass{};
    instructionClass.cbSize = sizeof(instructionClass);
    instructionClass.lpfnWndProc = InstructionWndProc;
    instructionClass.hInstance = hInst;
    instructionClass.lpszClassName =
        L"RamClipInstructionOverlayWindow";
    instructionClass.hCursor =
        LoadCursorW(nullptr, IDC_ARROW);

    if (!RegisterClassExW(&instructionClass)) {
        MessageBoxW(
            nullptr,
            L"说明栏窗口类注册失败。",
            kAppTitle,
            MB_ICONERROR
        );
        SafeRelease(g_dcTarget);
        SafeRelease(g_dwFactory);
        SafeRelease(g_d2dFactory);
        return 1;
    }

    g_hwnd = CreateWindowExW(
        WS_EX_TOPMOST |
            WS_EX_TOOLWINDOW |
            WS_EX_LAYERED |
            WS_EX_NOACTIVATE,
        wc.lpszClassName,
        kAppTitle,
        WS_POPUP,
        0,
        0,
        1,
        1,
        nullptr,
        nullptr,
        hInst,
        nullptr
    );

    if (!g_hwnd) {
        MessageBoxW(
            nullptr,
            L"窗口创建失败。",
            kAppTitle,
            MB_ICONERROR
        );
        SafeRelease(g_dcTarget);
        SafeRelease(g_dwFactory);
        SafeRelease(g_d2dFactory);
        return 1;
    }

    g_instructionHwnd = CreateWindowExW(
        WS_EX_TOPMOST |
            WS_EX_TOOLWINDOW |
            WS_EX_LAYERED |
            WS_EX_NOACTIVATE |
            WS_EX_TRANSPARENT,
        instructionClass.lpszClassName,
        L"RamClip Instructions",
        WS_POPUP,
        0,
        0,
        1,
        1,
        nullptr,
        nullptr,
        hInst,
        nullptr
    );

    if (!g_instructionHwnd) {
        MessageBoxW(
            nullptr,
            L"说明栏窗口创建失败。",
            kAppTitle,
            MB_ICONERROR
        );
        DestroyWindow(g_hwnd);
        g_hwnd = nullptr;
        SafeRelease(g_dcTarget);
        SafeRelease(g_dwFactory);
        SafeRelease(g_d2dFactory);
        return 1;
    }

    UpdateMonitorAndScale();

    const bool okToggle = !!RegisterHotKey(
        g_hwnd,
        HK_TOGGLE,
        MOD_ALT | MOD_NOREPEAT,
        VK_OEM_3
    );

    const bool okAdd = !!RegisterHotKey(
        g_hwnd,
        HK_ADD,
        MOD_ALT | MOD_NOREPEAT,
        '2'
    );

    const bool okExit = !!RegisterHotKey(
        g_hwnd,
        HK_EXIT,
        MOD_CONTROL | MOD_ALT | MOD_NOREPEAT,
        VK_OEM_3
    );

    const bool okGlobalCapture = !!RegisterHotKey(
        g_hwnd,
        HK_GLOBAL_CAPTURE,
        MOD_ALT | MOD_NOREPEAT,
        'C'
    );

    const bool okGlobalPaste = !!RegisterHotKey(
        g_hwnd,
        HK_GLOBAL_PASTE,
        MOD_ALT | MOD_NOREPEAT,
        'V'
    );

    const bool okGlobalDelete = !!RegisterHotKey(
        g_hwnd,
        HK_GLOBAL_DELETE,
        MOD_ALT | MOD_NOREPEAT,
        'Z'
    );

    const bool okPasteSelected = !!RegisterHotKey(
        g_hwnd,
        HK_PASTE,
        MOD_ALT | MOD_NOREPEAT,
        '5'
    );

    const bool okPasteDeleteSelected = !!RegisterHotKey(
        g_hwnd,
        HK_PASTE_DELETE,
        MOD_CONTROL | MOD_ALT | MOD_NOREPEAT,
        '1'
    );

    if (!okToggle ||
        !okAdd ||
        !okExit ||
        !okGlobalCapture ||
        !okGlobalPaste ||
        !okGlobalDelete ||
        !okPasteSelected ||
        !okPasteDeleteSelected) {
        MessageBoxW(
            nullptr,
            L"至少一个全局快捷键注册失败，可能与其他软件冲突。\n\n"
            L"需要：Alt+`、Alt+2、Ctrl+Alt+`、Alt+C、Alt+V、Alt+Z、Alt+5、Ctrl+Alt+1。",
            kAppTitle,
            MB_ICONWARNING
        );
    }

    MessageBoxW(
        nullptr,
        L"按下Alt+`打开/关闭界面",
        kAppTitle,
        MB_OK | MB_ICONINFORMATION
    );

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    DestroyInstructionSurface();
    DestroySurface();

    SafeRelease(g_textFormat);
    SafeRelease(g_smallFormat);
    SafeRelease(g_dcTarget);
    SafeRelease(g_dwFactory);
    SafeRelease(g_d2dFactory);

    return static_cast<int>(msg.wParam);
}
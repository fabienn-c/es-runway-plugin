#include "ScreenCapture.h"

#include <algorithm>
#include <fstream>

namespace capture {

namespace {

int Stride(int width)
{
    return (width * 3 + 3) & ~3;
}

}

bool Grab(HWND h, RECT area, Image& out)
{
    const int width = area.right - area.left, height = area.bottom - area.top;
    if (width <= 0 || height <= 0)
        return false;

    HDC windowDc = GetDC(h);
    HDC memoryDc = CreateCompatibleDC(windowDc);
    HBITMAP bitmap = CreateCompatibleBitmap(windowDc, width, height);
    HGDIOBJ previous = SelectObject(memoryDc, bitmap);
    BitBlt(memoryDc, 0, 0, width, height, windowDc, area.left, area.top, SRCCOPY);
    SelectObject(memoryDc, previous);

    BITMAPINFO info = {};
    info.bmiHeader.biSize = sizeof(info.bmiHeader);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;  // top-down
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 24;
    info.bmiHeader.biCompression = BI_RGB;
    std::vector<uint8_t> padded(static_cast<size_t>(Stride(width)) * height);
    const int rows = GetDIBits(memoryDc, bitmap, 0, height, padded.data(), &info, DIB_RGB_COLORS);

    DeleteObject(bitmap);
    DeleteDC(memoryDc);
    ReleaseDC(h, windowDc);
    if (rows != height)
        return false;

    out.width = width;
    out.height = height;
    out.bgr.resize(static_cast<size_t>(width) * height * 3);
    for (int y = 0; y < height; ++y)
        std::copy_n(&padded[static_cast<size_t>(y) * Stride(width)], width * 3, &out.bgr[static_cast<size_t>(y) * width * 3]);
    return true;
}

bool SaveBmp(const std::string& path, const Image& image)
{
    if (image.Empty())
        return false;
    const int stride = Stride(image.width);
    BITMAPFILEHEADER file = {};
    BITMAPINFOHEADER info = {};
    info.biSize = sizeof(info);
    info.biWidth = image.width;
    info.biHeight = image.height;  // bottom-up, the most compatible form
    info.biPlanes = 1;
    info.biBitCount = 24;
    info.biCompression = BI_RGB;
    info.biSizeImage = static_cast<DWORD>(stride) * image.height;
    file.bfType = 0x4D42;  // "BM"
    file.bfOffBits = sizeof(file) + sizeof(info);
    file.bfSize = file.bfOffBits + info.biSizeImage;

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char*>(&file), sizeof(file));
    out.write(reinterpret_cast<const char*>(&info), sizeof(info));
    std::vector<char> row(stride, 0);
    for (int y = image.height - 1; y >= 0; --y)
    {
        std::copy_n(reinterpret_cast<const char*>(&image.bgr[static_cast<size_t>(y) * image.width * 3]), image.width * 3,
                    row.data());
        out.write(row.data(), stride);
    }
    return static_cast<bool>(out);
}

}

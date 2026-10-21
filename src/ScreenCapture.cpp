#include "ScreenCapture.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <limits>

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

Image Crop(const Image& image, int x, int y, int width, int height)
{
    Image out;
    x = std::clamp(x, 0, std::max(0, image.width - 1));
    y = std::clamp(y, 0, std::max(0, image.height - 1));
    out.width = std::min(width, image.width - x);
    out.height = std::min(height, image.height - y);
    if (out.Empty())
        return Image{};
    out.bgr.resize(static_cast<size_t>(out.width) * out.height * 3);
    for (int row = 0; row < out.height; ++row)
        std::copy_n(&image.bgr[(static_cast<size_t>(y + row) * image.width + x) * 3], out.width * 3,
                    &out.bgr[static_cast<size_t>(row) * out.width * 3]);
    return out;
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

bool LoadBmp(const std::string& path, Image& image)
{
    std::ifstream in(path, std::ios::binary);
    BITMAPFILEHEADER file = {};
    BITMAPINFOHEADER info = {};
    if (!in.read(reinterpret_cast<char*>(&file), sizeof(file)) || !in.read(reinterpret_cast<char*>(&info), sizeof(info)))
        return false;
    if (file.bfType != 0x4D42 || info.biBitCount != 24 || info.biCompression != BI_RGB)
        return false;
    const int width = info.biWidth, height = std::abs(info.biHeight);
    const bool bottomUp = info.biHeight > 0;
    const int stride = Stride(width);
    in.seekg(file.bfOffBits);
    std::vector<char> row(stride);
    image.width = width;
    image.height = height;
    image.bgr.assign(static_cast<size_t>(width) * height * 3, 0);
    for (int i = 0; i < height; ++i)
    {
        if (!in.read(row.data(), stride))
            return false;
        const int y = bottomUp ? height - 1 - i : i;
        std::copy_n(row.data(), width * 3, reinterpret_cast<char*>(&image.bgr[static_cast<size_t>(y) * width * 3]));
    }
    return true;
}

bool FindBest(const Image& haystack, const Image& needle, POINT& at, double& meanDifference)
{
    if (haystack.Empty() || needle.Empty() || needle.width > haystack.width || needle.height > haystack.height)
        return false;
    const long long samples = static_cast<long long>(needle.width) * needle.height * 3;
    long long best = std::numeric_limits<long long>::max();
    for (int y = 0; y + needle.height <= haystack.height; ++y)
    {
        for (int x = 0; x + needle.width <= haystack.width; ++x)
        {
            long long sum = 0;
            for (int row = 0; row < needle.height && sum < best; ++row)
            {
                const uint8_t* a = &haystack.bgr[(static_cast<size_t>(y + row) * haystack.width + x) * 3];
                const uint8_t* b = &needle.bgr[static_cast<size_t>(row) * needle.width * 3];
                for (int i = 0; i < needle.width * 3; ++i)
                    sum += std::abs(static_cast<int>(a[i]) - static_cast<int>(b[i]));
            }
            if (sum < best)
            {
                best = sum;
                at = POINT{x, y};
            }
        }
    }
    meanDifference = static_cast<double>(best) / samples;
    return true;
}

}

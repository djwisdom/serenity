/*
 * Copyright (c) 2021, the SerenityOS developers.
 * Copyright (c) 2021, Brian Gianforcaro <bgianf@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/ByteString.h>
#include <LibCore/MappedFile.h>
#include <LibGfx/ICC/Profile.h>
#include <LibGfx/ImageFormats/BMPLoader.h>
#include <LibGfx/ImageFormats/DDSLoader.h>
#include <LibGfx/ImageFormats/GIFLoader.h>
#include <LibGfx/ImageFormats/ICOLoader.h>
#include <LibGfx/ImageFormats/ILBMLoader.h>
#include <LibGfx/ImageFormats/ImageDecoder.h>
#include <LibGfx/ImageFormats/JBIG2Loader.h>
#include <LibGfx/ImageFormats/JPEG2000BitplaneDecoding.h>
#include <LibGfx/ImageFormats/JPEG2000InverseDiscreteWaveletTransform.h>
#include <LibGfx/ImageFormats/JPEG2000Loader.h>
#include <LibGfx/ImageFormats/JPEG2000ProgressionIterators.h>
#include <LibGfx/ImageFormats/JPEG2000TagTree.h>
#include <LibGfx/ImageFormats/JPEGLoader.h>
#include <LibGfx/ImageFormats/JPEGXLLoader.h>
#include <LibGfx/ImageFormats/PAMLoader.h>
#include <LibGfx/ImageFormats/PBMLoader.h>
#include <LibGfx/ImageFormats/PGMLoader.h>
#include <LibGfx/ImageFormats/PNGLoader.h>
#include <LibGfx/ImageFormats/PPMLoader.h>
#include <LibGfx/ImageFormats/QMArithmeticDecoder.h>
#include <LibGfx/ImageFormats/TGALoader.h>
#include <LibGfx/ImageFormats/TIFFLoader.h>
#include <LibGfx/ImageFormats/TIFFMetadata.h>
#include <LibGfx/ImageFormats/TinyVGLoader.h>
#include <LibGfx/ImageFormats/WebPLoader.h>
#include <LibTest/TestCase.h>
#include <stdio.h>
#include <string.h>

#ifdef AK_OS_SERENITY
#    define TEST_INPUT(x) ("/usr/Tests/LibGfx/test-inputs/" x)
#else
#    define TEST_INPUT(x) ("test-inputs/" x)
#endif

static ErrorOr<Gfx::ImageFrameDescriptor> expect_single_frame(Gfx::ImageDecoderPlugin& plugin_decoder)
{
    EXPECT_EQ(plugin_decoder.frame_count(), 1u);
    EXPECT(!plugin_decoder.is_animated());
    EXPECT(!plugin_decoder.loop_count());

    auto frame = TRY(plugin_decoder.frame(0));
    EXPECT_EQ(frame.duration, 0);
    return frame;
}

static ErrorOr<Gfx::ImageFrameDescriptor> expect_single_frame_of_size(Gfx::ImageDecoderPlugin& plugin_decoder, Gfx::IntSize size)
{
    EXPECT_EQ(plugin_decoder.size(), size);
    auto frame = TRY(expect_single_frame(plugin_decoder));
    EXPECT_EQ(frame.image->size(), size);
    return frame;
}

TEST_CASE(test_bmp)
{
    auto file = TRY_OR_FAIL(Core::MappedFile::map(TEST_INPUT("bmp/rgba32-1.bmp"sv)));
    EXPECT(Gfx::BMPImageDecoderPlugin::sniff(file->bytes()));
    auto plugin_decoder = TRY_OR_FAIL(Gfx::BMPImageDecoderPlugin::create(file->bytes()));

    TRY_OR_FAIL(expect_single_frame(*plugin_decoder));
}

TEST_CASE(test_bmp_top_down)
{
    auto file = TRY_OR_FAIL(Core::MappedFile::map(TEST_INPUT("bmp/top-down.bmp"sv)));
    EXPECT(Gfx::BMPImageDecoderPlugin::sniff(file->bytes()));
    auto plugin_decoder = TRY_OR_FAIL(Gfx::BMPImageDecoderPlugin::create(file->bytes()));

    TRY_OR_FAIL(expect_single_frame(*plugin_decoder));
}

TEST_CASE(test_bmp_1bpp)
{
    auto file = TRY_OR_FAIL(Core::MappedFile::map(TEST_INPUT("bmp/bitmap.bmp"sv)));
    EXPECT(Gfx::BMPImageDecoderPlugin::sniff(file->bytes()));
    auto plugin_decoder = TRY_OR_FAIL(Gfx::BMPImageDecoderPlugin::create(file->bytes()));

    auto frame = TRY_OR_FAIL(expect_single_frame_of_size(*plugin_decoder, { 399, 400 }));
    EXPECT_EQ(frame.image->begin()[0], 0xff'ff'ff'ff);
}

TEST_CASE(test_ico_malformed_frame)
{
    Array test_inputs = {
        TEST_INPUT("ico/oss-fuzz-testcase-62541.ico"sv),
        TEST_INPUT("ico/oss-fuzz-testcase-63177.ico"sv),
        TEST_INPUT("ico/oss-fuzz-testcase-63357.ico"sv)
    };

    for (auto test_input : test_inputs) {
        auto file = TRY_OR_FAIL(Core::MappedFile::map(test_input));
        auto plugin_decoder = TRY_OR_FAIL(Gfx::ICOImageDecoderPlugin::create(file->bytes()));
        auto frame_or_error = plugin_decoder->frame(0);
        EXPECT(frame_or_error.is_error());
    }
}

TEST_CASE(test_gif)
{
    auto file = TRY_OR_FAIL(Core::MappedFile::map(TEST_INPUT("download-animation.gif"sv)));
    EXPECT(Gfx::GIFImageDecoderPlugin::sniff(file->bytes()));
    auto plugin_decoder = TRY_OR_FAIL(Gfx::GIFImageDecoderPlugin::create(file->bytes()));

    EXPECT(plugin_decoder->frame_count());
    EXPECT(plugin_decoder->is_animated());
    EXPECT(!plugin_decoder->loop_count());

    auto frame = TRY_OR_FAIL(plugin_decoder->frame(1));
    EXPECT(frame.duration == 400);
}

TEST_CASE(test_gif_without_global_color_table)
{
    Array<u8, 35> gif_data {
        // Header (6 bytes): "GIF89a"
        0x47,
        0x49,
        0x46,
        0x38,
        0x39,
        0x61,

        // Logical Screen Descriptor (7 bytes)
        0x01,
        0x00, // Width (1)
        0x01,
        0x00, // Height (1)
        0x00, // Packed fields (NOTE: the MSB here is the Global Color Table flag!)
        0x00, // Background Color Index
        0x00, // Pixel Aspect Ratio

        // Image Descriptor (10 bytes)
        0x2C,
        0x00,
        0x00,
        0x00,
        0x00,
        0x01,
        0x00,
        0x01,
        0x00,
        0x80,

        // Local Color Table (6 bytes: 2 colors, 3 bytes per color)
        0x00,
        0x00,
        0x00, // Color 1: Black (RGB: 0, 0, 0)
        0xff,
        0x00,
        0x00, // Color 2: Red (RGB: 255, 0, 0)

        // Image Data (8 bytes)
        0x02, // LZW Minimum Code Size
        0x02, // Data Sub-block size (2 bytes)
        0x4C,
        0x01, // Image Data
        0x00, // Data Sub-block Terminator

        // Trailer (1 byte)
        0x3B,
    };

    auto plugin_decoder = TRY_OR_FAIL(Gfx::GIFImageDecoderPlugin::create(gif_data));
    EXPECT_EQ(plugin_decoder->frame_count(), 1u);
    auto frame = TRY_OR_FAIL(plugin_decoder->frame(0));
    EXPECT(frame.image);
    EXPECT_EQ(frame.image->size(), Gfx::IntSize(1, 1));
    EXPECT_EQ(frame.image->get_pixel(0, 0), Gfx::Color::NamedColor::Red);
}

TEST_CASE(test_not_ico)
{
    auto file = TRY_OR_FAIL(Core::MappedFile::map(TEST_INPUT("png/buggie.png"sv)));
    EXPECT(!Gfx::ICOImageDecoderPlugin::sniff(file->bytes()));
    EXPECT(Gfx::ICOImageDecoderPlugin::create(file->bytes()).is_error());
}

TEST_CASE(test_bmp_embedded_in_ico)
{
    auto file = TRY_OR_FAIL(Core::MappedFile::map(TEST_INPUT("ico/serenity.ico"sv)));
    EXPECT(Gfx::ICOImageDecoderPlugin::sniff(file->bytes()));
    auto plugin_decoder = TRY_OR_FAIL(Gfx::ICOImageDecoderPlugin::create(file->bytes()));

    auto frame = TRY_OR_FAIL(expect_single_frame_of_size(*plugin_decoder, { 16, 16 }));
    EXPECT_EQ(frame.image->get_pixel(0, 0), Gfx::Color::NamedColor::Transparent);
    EXPECT_EQ(frame.image->get_pixel(7, 4), Gfx::Color(161, 0, 0));
}

TEST_CASE(test_malformed_maskless_ico)
{
    auto file = TRY_OR_FAIL(Core::MappedFile::map(TEST_INPUT("ico/malformed_maskless.ico"sv)));
    EXPECT(Gfx::ICOImageDecoderPlugin::sniff(file->bytes()));
    auto plugin_decoder = TRY_OR_FAIL(Gfx::ICOImageDecoderPlugin::create(file->bytes()));

    auto frame = TRY_OR_FAIL(expect_single_frame_of_size(*plugin_decoder, { 16, 16 }));
    EXPECT_EQ(frame.image->get_pixel(0, 0), Gfx::Color::NamedColor::Transparent);
    EXPECT_EQ(frame.image->get_pixel(7, 4), Gfx::Color(161, 0, 0));
}

TEST_CASE(test_ilbm)
{
    auto file = TRY_OR_FAIL(Core::MappedFile::map(TEST_INPUT("ilbm/gradient.iff"sv)));
    EXPECT(Gfx::ILBMImageDecoderPlugin::sniff(file->bytes()));
    auto plugin_decoder = TRY_OR_FAIL(Gfx::ILBMImageDecoderPlugin::create(file->bytes()));

    auto frame = TRY_OR_FAIL(expect_single_frame_of_size(*plugin_decoder, { 320, 200 }));

    EXPECT_EQ(frame.image->get_pixel(8, 0), Gfx::Color(0xee, 0xbb, 0, 255));
}

TEST_CASE(test_ilbm_uncompressed)
{
    auto file = TRY_OR_FAIL(Core::MappedFile::map(TEST_INPUT("ilbm/gradient-uncompressed.iff"sv)));
    EXPECT(Gfx::ILBMImageDecoderPlugin::sniff(file->bytes()));
    auto plugin_decoder = TRY_OR_FAIL(Gfx::ILBMImageDecoderPlugin::create(file->bytes()));

    auto frame = TRY_OR_FAIL(expect_single_frame_of_size(*plugin_decoder, { 320, 200 }));

    EXPECT_EQ(frame.image->get_pixel(8, 0), Gfx::Color(0xee, 0xbb, 0, 255));
}

TEST_CASE(test_ilbm_ham6)
{
    auto file = TRY_OR_FAIL(Core::MappedFile::map(TEST_INPUT("ilbm/ham6.iff"sv)));
    EXPECT(Gfx::ILBMImageDecoderPlugin::sniff(file->bytes()));
    auto plugin_decoder = TRY_OR_FAIL(Gfx::ILBMImageDecoderPlugin::create(file->bytes()));

    auto frame = TRY_OR_FAIL(expect_single_frame_of_size(*plugin_decoder, { 256, 256 }));

    EXPECT_EQ(frame.image->get_pixel(77, 107), Gfx::Color(0xf0, 0x40, 0x40, 0xff));
}

TEST_CASE(test_ilbm_dos)
{
    auto file = TRY_OR_FAIL(Core::MappedFile::map(TEST_INPUT("ilbm/serenity.lbm"sv)));
    EXPECT(Gfx::ILBMImageDecoderPlugin::sniff(file->bytes()));
    auto plugin_decoder = TRY_OR_FAIL(Gfx::ILBMImageDecoderPlugin::create(file->bytes()));

    auto frame = TRY_OR_FAIL(expect_single_frame_of_size(*plugin_decoder, { 640, 480 }));

    EXPECT_EQ(frame.image->get_pixel(315, 134), Gfx::Color::NamedColor::Red);
}

TEST_CASE(test_24bit)
{
    auto file = TRY_OR_FAIL(Core::MappedFile::map(TEST_INPUT("ilbm/serenity-24bit.iff"sv)));
    EXPECT(Gfx::ILBMImageDecoderPlugin::sniff(file->bytes()));
    auto plugin_decoder = TRY_OR_FAIL(Gfx::ILBMImageDecoderPlugin::create(file->bytes()));

    auto frame = TRY_OR_FAIL(expect_single_frame_of_size(*plugin_decoder, { 640, 640 }));

    EXPECT_EQ(frame.image->get_pixel(158, 270), Gfx::Color(0xee, 0x3d, 0x3c, 255));
}

TEST_CASE(test_brush_transparent_color)
{
    auto file = TRY_OR_FAIL(Core::MappedFile::map(TEST_INPUT("ilbm/brush-transparent-color.iff"sv)));
    EXPECT(Gfx::ILBMImageDecoderPlugin::sniff(file->bytes()));
    auto plugin_decoder = TRY_OR_FAIL(Gfx::ILBMImageDecoderPlugin::create(file->bytes()));

    auto frame = TRY_OR_FAIL(expect_single_frame_of_size(*plugin_decoder, { 266, 309 }));

    EXPECT_EQ(frame.image->get_pixel(114, 103), Gfx::Color::NamedColor::Black);
}

TEST_CASE(test_small_24bit)
{
    auto file = TRY_OR_FAIL(Core::MappedFile::map(TEST_INPUT("ilbm/small-24bit.iff"sv)));
    EXPECT(Gfx::ILBMImageDecoderPlugin::sniff(file->bytes()));
    auto plugin_decoder = TRY_OR_FAIL(Gfx::ILBMImageDecoderPlugin::create(file->bytes()));

    auto frame = TRY_OR_FAIL(expect_single_frame_of_size(*plugin_decoder, { 10, 10 }));

    EXPECT_EQ(frame.image->get_pixel(0, 4), Gfx::Color(1, 0, 1, 255));
}

TEST_CASE(test_stencil_mask)
{
    auto file = TRY_OR_FAIL(Core::MappedFile::map(TEST_INPUT("ilbm/test-stencil.iff"sv)));
    EXPECT(Gfx::ILBMImageDecoderPlugin::sniff(file->bytes()));
    auto plugin_decoder = TRY_OR_FAIL(Gfx::ILBMImageDecoderPlugin::create(file->bytes()));

    auto frame = TRY_OR_FAIL(expect_single_frame_of_size(*plugin_decoder, { 320, 200 }));

    EXPECT_EQ(frame.image->get_pixel(0, 4), Gfx::Color(0, 0, 0, 255));
}

TEST_CASE(test_ilbm_malformed_header)
{
    Array test_inputs = {
        TEST_INPUT("ilbm/truncated-bmhd-chunk.iff"sv)
    };

    for (auto test_input : test_inputs) {
        auto file = TRY_OR_FAIL(Core::MappedFile::map(test_input));
        auto plugin_decoder_or_error = Gfx::ILBMImageDecoderPlugin::create(file->bytes());
        EXPECT(plugin_decoder_or_error.is_error());
    }
}

TEST_CASE(test_ilbm_malformed_frame)
{
    Array test_inputs = {
        TEST_INPUT("ilbm/incorrect-cmap-size.iff"sv),
        TEST_INPUT("ilbm/incorrect-uncompressed-size.iff"sv),
        TEST_INPUT("ilbm/missing-body-chunk.iff"sv)
    };

    for (auto test_input : test_inputs) {
        auto file = TRY_OR_FAIL(Core::MappedFile::map(test_input));
        auto plugin_decoder = TRY_OR_FAIL(Gfx::ILBMImageDecoderPlugin::create(file->bytes()));
        auto frame_or_error = plugin_decoder->frame(0);
        EXPECT(frame_or_error.is_error());
    }
}

TEST_CASE(test_jbig2_black_47x23)
{
    auto file = TRY_OR_FAIL(Core::MappedFile::map(TEST_INPUT("jbig2/black_47x23.jbig2"sv)));
    EXPECT(Gfx::JBIG2ImageDecoderPlugin::sniff(file->bytes()));
    auto plugin_decoder = TRY_OR_FAIL(Gfx::JBIG2ImageDecoderPlugin::create(file->bytes()));

    auto frame = TRY_OR_FAIL(expect_single_frame_of_size(*plugin_decoder, { 47, 23 }));
    for (auto pixel : *frame.image)
        EXPECT_EQ(pixel, Gfx::Color(Gfx::Color::Black).value());
}

TEST_CASE(test_jbig2_white_47x23)
{
    auto file = TRY_OR_FAIL(Core::MappedFile::map(TEST_INPUT("jbig2/white_47x23.jbig2"sv)));
    EXPECT(Gfx::JBIG2ImageDecoderPlugin::sniff(file->bytes()));
    auto plugin_decoder = TRY_OR_FAIL(Gfx::JBIG2ImageDecoderPlugin::create(file->bytes()));

    auto frame = TRY_OR_FAIL(expect_single_frame_of_size(*plugin_decoder, { 47, 23 }));
    for (auto pixel : *frame.image)
        EXPECT_EQ(pixel, Gfx::Color(Gfx::Color::White).value());
}

TEST_CASE(test_jbig2_decode)
{
    auto bmp_file = TRY_OR_FAIL(Core::MappedFile::map(TEST_INPUT("bmp/bitmap.bmp"sv)));
    auto bmp_plugin_decoder = TRY_OR_FAIL(Gfx::BMPImageDecoderPlugin::create(bmp_file->bytes()));
    auto bmp_frame = TRY_OR_FAIL(expect_single_frame_of_size(*bmp_plugin_decoder, { 399, 400 }));

    Array test_inputs = {
        TEST_INPUT("jbig2/bitmap.jbig2"sv),
        TEST_INPUT("jbig2/bitmap-customat.jbig2"sv),
        TEST_INPUT("jbig2/bitmap-tpgdon.jbig2"sv),
        TEST_INPUT("jbig2/bitmap-customat-tpgdon.jbig2"sv),
        TEST_INPUT("jbig2/bitmap-mmr.jbig2"sv),
        TEST_INPUT("jbig2/bitmap-template1.jbig2"sv),
        TEST_INPUT("jbig2/bitmap-template1-customat.jbig2"sv),
        TEST_INPUT("jbig2/bitmap-template1-tpgdon.jbig2"sv),
        TEST_INPUT("jbig2/bitmap-template1-customat-tpgdon.jbig2"sv),
        TEST_INPUT("jbig2/bitmap-template2.jbig2"sv),
        TEST_INPUT("jbig2/bitmap-template2-customat.jbig2"sv),
        TEST_INPUT("jbig2/bitmap-template2-tpgdon.jbig2"sv),
        TEST_INPUT("jbig2/bitmap-template2-customat-tpgdon.jbig2"sv),
        TEST_INPUT("jbig2/bitmap-template3.jbig2"sv),
        TEST_INPUT("jbig2/bitmap-template3-customat.jbig2"sv),
        TEST_INPUT("jbig2/bitmap-template3-tpgdon.jbig2"sv),
        TEST_INPUT("jbig2/bitmap-template3-customat-tpgdon.jbig2"sv),
        TEST_INPUT("jbig2/bitmap-symbol.jbig2"sv),
        TEST_INPUT("jbig2/bitmap-symbol-negative-sbdsoffset.jbig2"sv),
        TEST_INPUT("jbig2/bitmap-symbol-textrefine.jbig2"sv),
        TEST_INPUT("jbig2/bitmap-symbol-textrefine-customat.jbig2"sv),
        TEST_INPUT("jbig2/symbol-textrefine-negative-delta-width.jbig2"sv),
        TEST_INPUT("jbig2/bitmap-symbol-symbolrefine.jbig2"sv),
        TEST_INPUT("jbig2/bitmap-symbol-textbottomleft.jbig2"sv),
        TEST_INPUT("jbig2/bitmap-symbol-textbottomlefttranspose.jbig2"sv),
        TEST_INPUT("jbig2/bitmap-symbol-textbottomright.jbig2"sv),
        TEST_INPUT("jbig2/bitmap-symbol-textbottomrighttranspose.jbig2"sv),
        TEST_INPUT("jbig2/bitmap-symbol-texttopright.jbig2"sv),
        TEST_INPUT("jbig2/bitmap-symbol-texttoprighttranspose.jbig2"sv),
        TEST_INPUT("jbig2/bitmap-symbol-texttranspose.jbig2"sv),
        // Missing tests, because there's no easy way to generate them:
        // - halftone (code support added in #23864)
        // - lossless halftone (code support added in #26043)
        // - rotated halftone (code support added in #26044)
        // - halftone with graymap with > 256 entries (code support added in #26044 too)
        // - striping, especially with initially unknown page height (code support added in #26067)
        // - huffman symbol regions (code support added in #26068)
        // - huffman text regions (code support added in #26075)
        // - huffman regions with custom huffman tables (code support added in #26078, #26081)
        // - coverage for different segment combination operators (or and xor xnor replace),
        //   with both background colors
        // Missing tests for things that aren't implemented yet:
        // - halftone with MMR graymap
        // - symbols with REFAGGNINST > 1
        // - intermediate regions
        // - standalone refinement regions
        // - exttemplate
        // - colors
        // (test_annex_h_jbig2 covers a subset of these, but having standalone files would be nice.)
    };

    for (auto test_input : test_inputs) {
        auto file = TRY_OR_FAIL(Core::MappedFile::map(test_input));
        EXPECT(Gfx::JBIG2ImageDecoderPlugin::sniff(file->bytes()));
        auto plugin_decoder = TRY_OR_FAIL(Gfx::JBIG2ImageDecoderPlugin::create(file->bytes()));

        auto frame = TRY_OR_FAIL(expect_single_frame_of_size(*plugin_decoder, { 399, 400 }));

        for (int y = 0; y < frame.image->height(); ++y)
            for (int x = 0; x < frame.image->width(); ++x)
                EXPECT_EQ(frame.image->get_pixel(x, y), bmp_frame.image->get_pixel(x, y));
    }
}

TEST_CASE(test_annex_h_jbig2)
{
    // https://www.itu.int/rec/T-REC-T.88-201808-I
    // H.1 Datastream example
    auto file = TRY_OR_FAIL(Core::MappedFile::map(TEST_INPUT("jbig2/annex-h.jbig2"sv)));
    EXPECT(Gfx::JBIG2ImageDecoderPlugin::sniff(file->bytes()));
    auto decoder = TRY_OR_FAIL(Gfx::JBIG2ImageDecoderPlugin::create(file->bytes()));

    EXPECT_EQ(decoder->frame_count(), 3u);

    // FIXME: Decode this successfully.
    EXPECT(decoder->frame(0).is_error());

    auto frame_2 = TRY_OR_FAIL(decoder->frame(1));
    EXPECT_EQ(frame_2.image->size(), Gfx::IntSize(64, 56));

    // FIXME: Decode this successfully.
    EXPECT(decoder->frame(2).is_error());
}

TEST_CASE(test_qm_arithmetic_decoder)
{
    // https://www.itu.int/rec/T-REC-T.88-201808-I
    // H.2 Test sequence for arithmetic coder
    // clang-format off
    constexpr auto input = to_array<u8>({
        0x84, 0xC7, 0x3B, 0xFC, 0xE1, 0xA1, 0x43, 0x04,
        0x02, 0x20, 0x00, 0x00, 0x41, 0x0D, 0xBB, 0x86,
        0xF4, 0x31, 0x7F, 0xFF, 0x88, 0xFF, 0x37, 0x47,
        0x1A, 0xDB, 0x6A, 0xDF, 0xFF, 0xAC
        });
    constexpr auto output = to_array<u8>({
        0x00, 0x02, 0x00, 0x51, 0x00, 0x00, 0x00, 0xC0,
        0x03, 0x52, 0x87, 0x2A, 0xAA, 0xAA, 0xAA, 0xAA,
        0x82, 0xC0, 0x20, 0x00, 0xFC, 0xD7, 0x9E, 0xF6,
        0xBF, 0x7F, 0xED, 0x90, 0x4F, 0x46, 0xA3, 0xBF
    });
    // clang-format on

    // "For this entire test, a single value of CX is used. I(CX) is initially 0 and MPS(CX) is initially 0."
    Gfx::QMArithmeticDecoder::Context context { 0, 0 };
    auto decoder = MUST(Gfx::QMArithmeticDecoder::initialize(input));

    for (auto expected : output) {
        u8 actual = 0;
        for (size_t i = 0; i < 8; ++i) {
            actual <<= 1;
            actual |= static_cast<u8>(decoder.get_next_bit(context));
        }
        EXPECT_EQ(actual, expected);
    }
}

TEST_CASE(test_jpeg_sof0_one_scan)
{
    auto file = TRY_OR_FAIL(Core::MappedFile::map(TEST_INPUT("jpg/rgb24.jpg"sv)));
    EXPECT(Gfx::JPEGImageDecoderPlugin::sniff(file->bytes()));
    auto plugin_decoder = TRY_OR_FAIL(Gfx::JPEGImageDecoderPlugin::create(file->bytes()));

    TRY_OR_FAIL(expect_single_frame(*plugin_decoder));
}

TEST_CASE(test_jpeg_sof0_several_scans)
{
    auto file = TRY_OR_FAIL(Core::MappedFile::map(TEST_INPUT("jpg/several_scans.jpg"sv)));
    EXPECT(Gfx::JPEGImageDecoderPlugin::sniff(file->bytes()));
    auto plugin_decoder = TRY_OR_FAIL(Gfx::JPEGImageDecoderPlugin::create(file->bytes()));

    TRY_OR_FAIL(expect_single_frame_of_size(*plugin_decoder, { 592, 800 }));
}

TEST_CASE(test_jpeg_odd_mcu_restart_interval)
{
    auto file = TRY_OR_FAIL(Core::MappedFile::map(TEST_INPUT("jpg/odd-restart.jpg"sv)));
    EXPECT(Gfx::JPEGImageDecoderPlugin::sniff(file->bytes()));
    auto plugin_decoder = TRY_OR_FAIL(Gfx::JPEGImageDecoderPlugin::create(file->bytes()));

    TRY_OR_FAIL(expect_single_frame_of_size(*plugin_decoder, { 102, 77 }));
}

TEST_CASE(test_jpeg_rgb_components)
{
    auto file = TRY_OR_FAIL(Core::MappedFile::map(TEST_INPUT("jpg/rgb_components.jpg"sv)));
    EXPECT(Gfx::JPEGImageDecoderPlugin::sniff(file->bytes()));
    auto plugin_decoder = TRY_OR_FAIL(Gfx::JPEGImageDecoderPlugin::create(file->bytes()));

    TRY_OR_FAIL(expect_single_frame_of_size(*plugin_decoder, { 592, 800 }));
}

TEST_CASE(test_jpeg_ycck)
{
    Array test_inputs = {
        TEST_INPUT("jpg/ycck-1111.jpg"sv),
        TEST_INPUT("jpg/ycck-2111.jpg"sv),
        TEST_INPUT("jpg/ycck-2112.jpg"sv),
    };

    for (auto test_input : test_inputs) {
        auto file = TRY_OR_FAIL(Core::MappedFile::map(test_input));
        EXPECT(Gfx::JPEGImageDecoderPlugin::sniff(file->bytes()));
        auto plugin_decoder = TRY_OR_FAIL(Gfx::JPEGImageDecoderPlugin::create(file->bytes()));
        auto frame = TRY_OR_FAIL(expect_single_frame_of_size(*plugin_decoder, { 592, 800 }));

        // Compare difference between pixels so we don't depend on exact CMYK->RGB conversion behavior.
        // These two pixels are currently off by one in R.
        // FIXME: For 2111, they're off by way more.
        EXPECT(frame.image->get_pixel(6, 319).distance_squared_to(frame.image->get_pixel(6, 320)) < 1.0f / 255.0f);
    }
}

TEST_CASE(test_jpeg_sof2_spectral_selection)
{
    auto file = TRY_OR_FAIL(Core::MappedFile::map(TEST_INPUT("jpg/spectral_selection.jpg"sv)));
    EXPECT(Gfx::JPEGImageDecoderPlugin::sniff(file->bytes()));
    auto plugin_decoder = TRY_OR_FAIL(Gfx::JPEGImageDecoderPlugin::create(file->bytes()));

    TRY_OR_FAIL(expect_single_frame_of_size(*plugin_decoder, { 592, 800 }));
}

TEST_CASE(test_jpeg_sof0_several_scans_odd_number_mcu)
{
    auto file = TRY_OR_FAIL(Core::MappedFile::map(TEST_INPUT("jpg/several_scans_odd_number_mcu.jpg"sv)));
    EXPECT(Gfx::JPEGImageDecoderPlugin::sniff(file->bytes()));
    auto plugin_decoder = TRY_OR_FAIL(Gfx::JPEGImageDecoderPlugin::create(file->bytes()));

    TRY_OR_FAIL(expect_single_frame_of_size(*plugin_decoder, { 600, 600 }));
}

TEST_CASE(test_jpeg_sof2_successive_aproximation)
{
    auto file = TRY_OR_FAIL(Core::MappedFile::map(TEST_INPUT("jpg/successive_approximation.jpg"sv)));
    EXPECT(Gfx::JPEGImageDecoderPlugin::sniff(file->bytes()));
    auto plugin_decoder = TRY_OR_FAIL(Gfx::JPEGImageDecoderPlugin::create(file->bytes()));

    TRY_OR_FAIL(expect_single_frame_of_size(*plugin_decoder, { 600, 800 }));
}

TEST_CASE(test_jpeg_sof1_12bits)
{
    auto file = TRY_OR_FAIL(Core::MappedFile::map(TEST_INPUT("jpg/12-bit.jpg"sv)));
    EXPECT(Gfx::JPEGImageDecoderPlugin::sniff(file->bytes()));
    auto plugin_decoder = TRY_OR_FAIL(Gfx::JPEGImageDecoderPlugin::create(file->bytes()));

    TRY_OR_FAIL(expect_single_frame_of_size(*plugin_decoder, { 320, 240 }));
}

TEST_CASE(test_jpeg_sof2_12bits)
{
    auto file = TRY_OR_FAIL(Core::MappedFile::map(TEST_INPUT("jpg/12-bit-progressive.jpg"sv)));
    EXPECT(Gfx::JPEGImageDecoderPlugin::sniff(file->bytes()));
    auto plugin_decoder = TRY_OR_FAIL(Gfx::JPEGImageDecoderPlugin::create(file->bytes()));

    TRY_OR_FAIL(expect_single_frame_of_size(*plugin_decoder, { 320, 240 }));
}

TEST_CASE(test_jpeg_empty_icc)
{
    auto file = TRY_OR_FAIL(Core::MappedFile::map(TEST_INPUT("jpg/gradient_empty_icc.jpg"sv)));
    EXPECT(Gfx::JPEGImageDecoderPlugin::sniff(file->bytes()));
    auto plugin_decoder = TRY_OR_FAIL(Gfx::JPEGImageDecoderPlugin::create(file->bytes()));

    TRY_OR_FAIL(expect_single_frame_of_size(*plugin_decoder, { 80, 80 }));
}

TEST_CASE(test_jpeg_grayscale_with_app14)
{
    auto file = TRY_OR_FAIL(Core::MappedFile::map(TEST_INPUT("jpg/grayscale_app14.jpg"sv)));
    EXPECT(Gfx::JPEGImageDecoderPlugin::sniff(file->bytes()));
    auto plugin_decoder = TRY_OR_FAIL(Gfx::JPEGImageDecoderPlugin::create(file->bytes()));

    TRY_OR_FAIL(expect_single_frame_of_size(*plugin_decoder, { 80, 80 }));
}

TEST_CASE(test_jpeg_grayscale_with_weird_mcu_and_reset_marker)
{
    auto file = TRY_OR_FAIL(Core::MappedFile::map(TEST_INPUT("jpg/grayscale_mcu.jpg"sv)));
    EXPECT(Gfx::JPEGImageDecoderPlugin::sniff(file->bytes()));
    auto plugin_decoder = TRY_OR_FAIL(Gfx::JPEGImageDecoderPlugin::create(file->bytes()));

    TRY_OR_FAIL(expect_single_frame_of_size(*plugin_decoder, { 320, 240 }));
}

TEST_CASE(test_jpeg_malformed_header)
{
    Array test_inputs = {
        TEST_INPUT("jpg/oss-fuzz-testcase-59785.jpg"sv)
    };

    for (auto test_input : test_inputs) {
        auto file = TRY_OR_FAIL(Core::MappedFile::map(test_input));
        auto plugin_decoder_or_error = Gfx::JPEGImageDecoderPlugin::create(file->bytes());
        EXPECT(plugin_decoder_or_error.is_error());
    }
}

TEST_CASE(test_jpeg_malformed_frame)
{
    Array test_inputs = {
        TEST_INPUT("jpg/oss-fuzz-testcase-62584.jpg"sv),
        TEST_INPUT("jpg/oss-fuzz-testcase-63815.jpg"sv)
    };

    for (auto test_input : test_inputs) {
        auto file = TRY_OR_FAIL(Core::MappedFile::map(test_input));
        auto plugin_decoder = TRY_OR_FAIL(Gfx::JPEGImageDecoderPlugin::create(file->bytes()));
        auto frame_or_error = plugin_decoder->frame(0);
        EXPECT(frame_or_error.is_error());
    }
}

TEST_CASE(test_jpeg_random_bytes_between_segments)
{
    auto file = TRY_OR_FAIL(Core::MappedFile::map(TEST_INPUT("jpg/random_bytes_between_segments.jpg"sv)));
    EXPECT(Gfx::JPEGImageDecoderPlugin::sniff(file->bytes()));
    auto plugin_decoder = TRY_OR_FAIL(Gfx::JPEGImageDecoderPlugin::create(file->bytes()));

    TRY_OR_FAIL(expect_single_frame_of_size(*plugin_decoder, { 16, 16 }));
}

TEST_CASE(test_jpeg2000_spec_annex_j_10_bitplane_decoding)
{
    // J.10.4 Arithmetic-coded compressed data
    {
        // Table J.22 – Arithmetic decode of first code-block
        constexpr Array input = to_array<u8>({ 0x01, 0x8F, 0x0D, 0xC8, 0x75, 0x5D });

        Vector<float> output;
        output.resize(5);
        Gfx::JPEG2000::Span2D<float> result { output.span(), { 1, 5 }, 1 };

        // 16, 9, 3 are from J.10.3 Packet headers, Table J.20 – Decoding first packet header.
        TRY_OR_FAIL(Gfx::JPEG2000::decode_code_block(result, Gfx::JPEG2000::SubBand::HorizontalLowpassVerticalLowpass, 16, { input }, 9, 3));

        EXPECT_EQ(output[0], -26);
        EXPECT_EQ(output[1], -22);
        EXPECT_EQ(output[2], -30);
        EXPECT_EQ(output[3], -32);
        EXPECT_EQ(output[4], -19);
    }

    {
        // Table J.23 – Arithmetic decode of second code-block
        constexpr Array input = to_array<u8>({ 0x0F, 0xB1, 0x76 });

        Vector<float> output;
        output.resize(4);
        Gfx::JPEG2000::Span2D<float> result { output.span(), { 1, 4 }, 1 };

        // 7, 10, 7 are from J.10.3 Packet headers, Table J.21 – Decoding second packet header.
        TRY_OR_FAIL(Gfx::JPEG2000::decode_code_block(result, Gfx::JPEG2000::SubBand::HorizontalLowpassVerticalHighpass, 7, { input }, 10, 7));

        EXPECT_EQ(output[0], 1);
        EXPECT_EQ(output[1], 5);
        EXPECT_EQ(output[2], 1);
        EXPECT_EQ(output[3], 0);
    }
}

TEST_CASE(test_jpeg2000_spec_annex_j_10_inverse_discrete_wavelet_transform)
{
    constexpr Array ll_plane = to_array<float>({ -26, -22, -30, -32, -19 });
    constexpr Array lh_plane = to_array<float>({ 1, 5, 1, 0 });

    Gfx::JPEG2000::IDWTInput input;
    input.transformation = Gfx::JPEG2000::Transformation::Reversible_5_3_Filter;

    input.LL.rect = { 0, 0, 1, 5 };
    input.LL.data = { ll_plane.span(), input.LL.rect.size(), input.LL.rect.width() };

    Gfx::JPEG2000::IDWTSubBand lh_sub_band;
    lh_sub_band.rect = { 0, 0, 1, 4 };
    lh_sub_band.data = { lh_plane.span(), lh_sub_band.rect.size(), lh_sub_band.rect.width() };

    Gfx::JPEG2000::IDWTDecomposition decomposition;
    decomposition.ll_rect = { 0, 0, 1, 9 };
    decomposition.hl = { { 0, 0, 0, 5 }, { {}, { 0, 5 }, 0 } };
    decomposition.lh = lh_sub_band;
    decomposition.hh = { { 0, 0, 0, 4 }, { {}, { 0, 4 }, 0 } };
    input.decompositions.append(decomposition);

    auto output = TRY_OR_FAIL(Gfx::JPEG2000::IDWT(input));

    EXPECT_EQ(output.rect, Gfx::IntRect(0, 0, 1, 9));
    EXPECT_EQ(output.data.size(), 9u);

    // From J.10.5 Wavelet and level shift
    constexpr Array expected = to_array<float>({ 101, 103, 104, 105, 96, 97, 96, 102, 109 });
    for (int i = 0; i < 9; ++i)
        EXPECT_EQ(output.data[i] + 128, expected[i]);
}

TEST_CASE(test_jpeg2000_spec_annex_j_10)
{
    // J.10 An example of decoding showing intermediate steps
    // clang-format off
    constexpr Array data = to_array<u8>({
        0xFF, 0x4F, 0xFF, 0x51, 0x00, 0x29, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x09,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x09,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x07, 0x01, 0x01, 0xFF, 0x5C, 0x00,
        0x07, 0x40, 0x40, 0x48, 0x48, 0x50, 0xFF, 0x52, 0x00, 0x0C, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01,
        0x04, 0x04, 0x00, 0x01, 0xFF, 0x90, 0x00, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1E, 0x00, 0x01,
        0xFF, 0x93, 0xC7, 0xd4, 0x0C, 0x01, 0x8F, 0x0D, 0xC8, 0x75, 0x5D, 0xC0, 0x7C, 0x21, 0x80, 0x0F,
        0xB1, 0x76, 0xFF, 0xD9,
    });
    // clang-format on

    auto plugin_decoder = TRY_OR_FAIL(Gfx::JPEG2000ImageDecoderPlugin::create(data));
    auto frame = TRY_OR_FAIL(expect_single_frame_of_size(*plugin_decoder, { 1, 9 }));

    // "After the inverse 5-3 reversible filter and level shifting, the component samples in decimal are:"
    Array expected_values = { 101, 103, 104, 105, 96, 97, 96, 102, 109 };
    for (int i = 0; i < 9; ++i) {
        auto pixel = frame.image->get_pixel(0, i);
        EXPECT_EQ(pixel.red(), expected_values[i]);
        EXPECT_EQ(pixel.green(), expected_values[i]);
        EXPECT_EQ(pixel.blue(), expected_values[i]);
        EXPECT_EQ(pixel.alpha(), 0xff);
    }
}

TEST_CASE(test_jpeg2000_decode)
{
    auto png_file = TRY_OR_FAIL(Core::MappedFile::map(TEST_INPUT("jpeg2000/ref.png"sv)));
    auto png_plugin_decoder = TRY_OR_FAIL(Gfx::PNGImageDecoderPlugin::create(png_file->bytes()));
    auto ref_frame = TRY_OR_FAIL(expect_single_frame_of_size(*png_plugin_decoder, { 119, 101 }));

    Array test_inputs = {
        TEST_INPUT("jpeg2000/kakadu-lossless-rgba-u8-prog1-layers1-res6-mct.jp2"sv),
        TEST_INPUT("jpeg2000/openjpeg-lossless-rgba-u8-prog0-tile4x2-cblk4x16-tp3-layers3-res2.jp2"sv),
        TEST_INPUT("jpeg2000/openjpeg-lossless-rgba-u8-prog1-tile4x2-cblk4x16-tp3-layers3-res2.jp2"sv),
        TEST_INPUT("jpeg2000/openjpeg-lossless-rgba-u8-prog2-tile4x2-cblk4x16-tp3-layers3-res2.jp2"sv),
        TEST_INPUT("jpeg2000/openjpeg-lossless-rgba-u8-prog3-tile4x2-cblk4x16-tp3-layers3-res2.jp2"sv),
        TEST_INPUT("jpeg2000/openjpeg-lossless-rgba-u8-prog4-tile4x2-cblk4x16-tp3-layers3-res2.jp2"sv),
        TEST_INPUT("jpeg2000/jasper-rgba-u8-cbstyle-01-bypass.jp2"sv),
        TEST_INPUT("jpeg2000/jasper-rgba-u8-cbstyle-01-bypass-layers.jp2"sv),
        TEST_INPUT("jpeg2000/jasper-rgba-u8-cbstyle-01-bypass-finer-layers.jp2"sv),
        TEST_INPUT("jpeg2000/jasper-rgba-u8-cbstyle-02-resetprob.jp2"sv),
        TEST_INPUT("jpeg2000/jasper-rgba-u8-cbstyle-04-termall.jp2"sv),
        TEST_INPUT("jpeg2000/jasper-rgba-u8-cbstyle-05-bypass-termall.jp2"sv),
        TEST_INPUT("jpeg2000/jasper-rgba-u8-cbstyle-04-termall-layers.jp2"sv),
        TEST_INPUT("jpeg2000/jasper-rgba-u8-cbstyle-06-resetprob-termall.jp2"sv),
        TEST_INPUT("jpeg2000/jasper-rgba-u8-cbstyle-08-vcausal.jp2"sv),
        TEST_INPUT("jpeg2000/jasper-rgba-u8-cbstyle-16-pterm.jp2"sv),
        TEST_INPUT("jpeg2000/jasper-rgba-u8-cbstyle-32-segsym.jp2"sv),
        TEST_INPUT("jpeg2000/jasper-rgba-u8-cbstyle-36-termall-segsym.jp2"sv),
        TEST_INPUT("jpeg2000/jasper-rgba-u8-cbstyle-59-all-but-termall.jp2"sv),
        TEST_INPUT("jpeg2000/jasper-rgba-u8-cbstyle-63-all.jp2"sv),
        TEST_INPUT("jpeg2000/jasper-tile4x2-res5.jp2"sv),
        TEST_INPUT("jpeg2000/openjpeg-lossless-rgba-u8-prog0-SOP.jp2"sv),
        TEST_INPUT("jpeg2000/openjpeg-lossless-rgba-u8-prog0-EPH.jp2"sv),
        TEST_INPUT("jpeg2000/openjpeg-lossless-rgba-u8-prog0-EPH-SOP.jp2"sv),
        TEST_INPUT("jpeg2000/openjpeg-lossless-rgba-u8-prog0-EPH-empty-packets.jp2"sv),
        TEST_INPUT("jpeg2000/openjpeg-lossless-rgba-u8-PLT.jp2"sv),
        TEST_INPUT("jpeg2000/openjpeg-lossless-rgba-u8-TLM.jp2"sv),
        TEST_INPUT("jpeg2000/kakadu-lossless-rgba-u16-prog1-layers1-res6.jp2"sv),
    };

    for (auto test_input : test_inputs) {
        auto file = TRY_OR_FAIL(Core::MappedFile::map(test_input));
        EXPECT(Gfx::JPEG2000ImageDecoderPlugin::sniff(file->bytes()));
        auto plugin_decoder = TRY_OR_FAIL(Gfx::JPEG2000ImageDecoderPlugin::create(file->bytes()));

        auto frame = TRY_OR_FAIL(expect_single_frame_of_size(*plugin_decoder, { 119, 101 }));

        for (int y = 0; y < frame.image->height(); ++y)
            for (int x = 0; x < frame.image->width(); ++x)
                EXPECT_EQ(frame.image->get_pixel(x, y), ref_frame.image->get_pixel(x, y));
    }
}

TEST_CASE(test_jpeg2000_decode_4bpp)
{
    auto png_file = TRY_OR_FAIL(Core::MappedFile::map(TEST_INPUT("jpeg2000/ref.png"sv)));
    auto png_plugin_decoder = TRY_OR_FAIL(Gfx::PNGImageDecoderPlugin::create(png_file->bytes()));
    auto ref_frame = TRY_OR_FAIL(expect_single_frame_of_size(*png_plugin_decoder, { 119, 101 }));

    Array test_inputs = {
        TEST_INPUT("jpeg2000/openjpeg-lossless-rgba-u4.jp2"sv),
    };

    for (auto test_input : test_inputs) {
        auto file = TRY_OR_FAIL(Core::MappedFile::map(test_input));
        EXPECT(Gfx::JPEG2000ImageDecoderPlugin::sniff(file->bytes()));
        auto plugin_decoder = TRY_OR_FAIL(Gfx::JPEG2000ImageDecoderPlugin::create(file->bytes()));

        auto frame = TRY_OR_FAIL(expect_single_frame_of_size(*plugin_decoder, { 119, 101 }));

        for (int y = 0; y < frame.image->height(); ++y)
            for (int x = 0; x < frame.image->width(); ++x) {
                Gfx::Color expected = ref_frame.image->get_pixel(x, y);
                auto map = [](u8 x) {
                    // Simulates a round-trip through 4bpp.
                    return round_to<u8>(x / 17.f) * 17;
                };
                expected = Color(map(expected.red()), map(expected.green()), map(expected.blue()), map(expected.alpha()));
                EXPECT_EQ(frame.image->get_pixel(x, y), expected);
            }
    }
}

TEST_CASE(test_jpeg2000_decode_rgb)
{
    auto png_file = TRY_OR_FAIL(Core::MappedFile::map(TEST_INPUT("jpeg2000/ref-rgb.png"sv)));
    auto png_plugin_decoder = TRY_OR_FAIL(Gfx::PNGImageDecoderPlugin::create(png_file->bytes()));
    auto ref_frame = TRY_OR_FAIL(expect_single_frame_of_size(*png_plugin_decoder, { 119, 101 }));

    Array test_inputs = {
        TEST_INPUT("jpeg2000/kakadu-lossless-rgb-u8-prog1-layers1-res6-mct.jp2"sv),
        TEST_INPUT("jpeg2000/jasper-rgba-u8-solid-alpha-cbstyle-04-termall.jp2"sv),
    };

    for (auto test_input : test_inputs) {
        auto file = TRY_OR_FAIL(Core::MappedFile::map(test_input));
        EXPECT(Gfx::JPEG2000ImageDecoderPlugin::sniff(file->bytes()));
        auto plugin_decoder = TRY_OR_FAIL(Gfx::JPEG2000ImageDecoderPlugin::create(file->bytes()));
        auto frame = TRY_OR_FAIL(expect_single_frame_of_size(*plugin_decoder, { 119, 101 }));

        for (int y = 0; y < frame.image->height(); ++y)
            for (int x = 0; x < frame.image->width(); ++x)
                EXPECT_EQ(frame.image->get_pixel(x, y), ref_frame.image->get_pixel(x, y));
    }
}

TEST_CASE(test_jpeg2000_decode_greyscale_alpha)
{
    auto png_file = TRY_OR_FAIL(Core::MappedFile::map(TEST_INPUT("jpeg2000/ref-gray-alpha.png"sv)));
    auto png_plugin_decoder = TRY_OR_FAIL(Gfx::PNGImageDecoderPlugin::create(png_file->bytes()));
    auto ref_frame = TRY_OR_FAIL(expect_single_frame_of_size(*png_plugin_decoder, { 119, 101 }));

    Array test_inputs = {
        TEST_INPUT("jpeg2000/kakadu-lossless-gray-alpha-u8-prog1-layers1-res6.jp2"sv),
    };

    for (auto test_input : test_inputs) {
        auto file = TRY_OR_FAIL(Core::MappedFile::map(test_input));
        EXPECT(Gfx::JPEG2000ImageDecoderPlugin::sniff(file->bytes()));
        auto plugin_decoder = TRY_OR_FAIL(Gfx::JPEG2000ImageDecoderPlugin::create(file->bytes()));
        auto frame = TRY_OR_FAIL(expect_single_frame_of_size(*plugin_decoder, { 119, 101 }));

        for (int y = 0; y < frame.image->height(); ++y)
            for (int x = 0; x < frame.image->width(); ++x)
                EXPECT_EQ(frame.image->get_pixel(x, y), ref_frame.image->get_pixel(x, y));
    }
}

TEST_CASE(test_jpeg2000_decode_cmyk)
{
    auto tiff_file = TRY_OR_FAIL(Core::MappedFile::map(TEST_INPUT("jpeg2000/ref-cmyk.tif"sv)));
    auto tiff_plugin_decoder = TRY_OR_FAIL(Gfx::TIFFImageDecoderPlugin::create(tiff_file->bytes()));
    EXPECT_EQ(tiff_plugin_decoder->size(), Gfx::Size(119, 101));
    EXPECT_EQ(tiff_plugin_decoder->natural_frame_format(), Gfx::NaturalFrameFormat::CMYK);
    auto ref_cmyk_frame = TRY_OR_FAIL(tiff_plugin_decoder->cmyk_frame());

    Array test_inputs = {
        TEST_INPUT("jpeg2000/kakadu-lossless-cmyk-u8-prog1-layers1-res6.jp2"sv),
    };

    for (auto test_input : test_inputs) {
        auto file = TRY_OR_FAIL(Core::MappedFile::map(test_input));
        EXPECT(Gfx::JPEG2000ImageDecoderPlugin::sniff(file->bytes()));
        auto plugin_decoder = TRY_OR_FAIL(Gfx::JPEG2000ImageDecoderPlugin::create(file->bytes()));

        EXPECT_EQ(plugin_decoder->natural_frame_format(), Gfx::NaturalFrameFormat::CMYK);
        auto cmyk_frame = TRY_OR_FAIL(plugin_decoder->cmyk_frame());
        EXPECT_EQ(cmyk_frame->size(), Gfx::IntSize(119, 101));

        for (int y = 0; y < cmyk_frame->size().height(); ++y)
            for (int x = 0; x < cmyk_frame->size().width(); ++x) {
                // FIXME: The last three pixels do not decode right. They do not decode right in Preview.app either.
                // Likely Photoshop wrote a slightly wrong CMYK JPEG2000:
                // https://community.adobe.com/t5/photoshop-ecosystem-bugs/photoshop-writes-cmyk-jpeg2000-file-in-a-way-that-macos-s-preview-app-does-not-decode-correctly/idc-p/15180197
                if (y == 100 && x >= 116)
                    continue;

                EXPECT_EQ(cmyk_frame->scanline(y)[x], ref_cmyk_frame->scanline(y)[x]);
            }
    }
}

TEST_CASE(test_jpeg2000_decode_cmyk_small_raw)
{
    Array test_inputs = {
        TEST_INPUT("jpeg2000/cmyk-small.jpf"sv),
        TEST_INPUT("jpeg2000/cmyk-small-icc.jpf"sv),
    };

    for (auto test_input : test_inputs) {
        auto file = TRY_OR_FAIL(Core::MappedFile::map(test_input));
        EXPECT(Gfx::JPEG2000ImageDecoderPlugin::sniff(file->bytes()));
        auto plugin_decoder = TRY_OR_FAIL(Gfx::JPEG2000ImageDecoderPlugin::create(file->bytes()));

        EXPECT_EQ(plugin_decoder->natural_frame_format(), Gfx::NaturalFrameFormat::CMYK);
        auto cmyk_frame = TRY_OR_FAIL(plugin_decoder->cmyk_frame());
        EXPECT_EQ(cmyk_frame->size(), Gfx::IntSize(4, 2));
        EXPECT_EQ(cmyk_frame->scanline(0)[0], (Gfx::CMYK { 0, 0, 0, 0 }));
        EXPECT_EQ(cmyk_frame->scanline(0)[1], (Gfx::CMYK { 127, 127, 127, 0 }));
        EXPECT_EQ(cmyk_frame->scanline(0)[2], (Gfx::CMYK { 255, 255, 255, 0 }));
        EXPECT_EQ(cmyk_frame->scanline(0)[3], (Gfx::CMYK { 255, 255, 255, 255 }));
        EXPECT_EQ(cmyk_frame->scanline(1)[0], (Gfx::CMYK { 255, 0, 0, 0 }));
        EXPECT_EQ(cmyk_frame->scanline(1)[1], (Gfx::CMYK { 0, 255, 0, 0 }));
        EXPECT_EQ(cmyk_frame->scanline(1)[2], (Gfx::CMYK { 0, 0, 255, 0 }));
        EXPECT_EQ(cmyk_frame->scanline(1)[3], (Gfx::CMYK { 0, 0, 0, 255 }));
    }
}

TEST_CASE(test_jpeg2000_decode_greyscale)
{
    auto png_file = TRY_OR_FAIL(Core::MappedFile::map(TEST_INPUT("jpeg2000/ref-gray.png"sv)));
    auto png_plugin_decoder = TRY_OR_FAIL(Gfx::PNGImageDecoderPlugin::create(png_file->bytes()));
    auto ref_frame = TRY_OR_FAIL(expect_single_frame_of_size(*png_plugin_decoder, { 119, 101 }));

    Array test_inputs = {
        TEST_INPUT("jpeg2000/kakadu-lossless-gray-u8-prog1-layers1-res6.jp2"sv),
        TEST_INPUT("jpeg2000/kakadu-lossless-gray-u8-prog1-layers1-res6-icc.jp2"sv),
    };

    for (auto test_input : test_inputs) {
        auto file = TRY_OR_FAIL(Core::MappedFile::map(test_input));
        EXPECT(Gfx::JPEG2000ImageDecoderPlugin::sniff(file->bytes()));
        auto plugin_decoder = TRY_OR_FAIL(Gfx::JPEG2000ImageDecoderPlugin::create(file->bytes()));
        auto frame = TRY_OR_FAIL(expect_single_frame_of_size(*plugin_decoder, { 119, 101 }));

        for (int y = 0; y < frame.image->height(); ++y)
            for (int x = 0; x < frame.image->width(); ++x)
                EXPECT_EQ(frame.image->get_pixel(x, y), ref_frame.image->get_pixel(x, y));
    }
}

TEST_CASE(test_jpeg2000_decode_indexed)
{
    auto png_file = TRY_OR_FAIL(Core::MappedFile::map(TEST_INPUT("jpeg2000/ref-indexed.png"sv)));
    auto png_plugin_decoder = TRY_OR_FAIL(Gfx::PNGImageDecoderPlugin::create(png_file->bytes()));
    auto ref_frame = TRY_OR_FAIL(expect_single_frame_of_size(*png_plugin_decoder, { 119, 101 }));

    Array test_inputs = {
        TEST_INPUT("jpeg2000/openjpeg-lossless-indexed-u8-rgb-u8.jp2"sv),
    };

    for (auto test_input : test_inputs) {
        auto file = TRY_OR_FAIL(Core::MappedFile::map(test_input));
        EXPECT(Gfx::JPEG2000ImageDecoderPlugin::sniff(file->bytes()));
        auto plugin_decoder = TRY_OR_FAIL(Gfx::JPEG2000ImageDecoderPlugin::create(file->bytes()));
        auto frame = TRY_OR_FAIL(expect_single_frame_of_size(*plugin_decoder, { 119, 101 }));

        for (int y = 0; y < frame.image->height(); ++y)
            for (int x = 0; x < frame.image->width(); ++x)
                EXPECT_EQ(frame.image->get_pixel(x, y), ref_frame.image->get_pixel(x, y));
    }
}

TEST_CASE(test_jpeg2000_decode_indexed_small_raw)
{
    auto file = TRY_OR_FAIL(Core::MappedFile::map(TEST_INPUT("jpeg2000/indexed-small.jp2"sv)));
    EXPECT(Gfx::JPEG2000ImageDecoderPlugin::sniff(file->bytes()));
    auto plugin_decoder = TRY_OR_FAIL(Gfx::JPEG2000ImageDecoderPlugin::create(file->bytes()));

    auto frame = TRY_OR_FAIL(plugin_decoder->frame(0)).image;
    EXPECT_EQ(frame->size(), Gfx::IntSize(3, 2));
    EXPECT_EQ(frame->scanline(0)[0], Gfx::Color(255, 0, 0).value());
    EXPECT_EQ(frame->scanline(0)[1], Gfx::Color(0, 255, 0).value());
    EXPECT_EQ(frame->scanline(0)[2], Gfx::Color(0, 0, 255).value());
    EXPECT_EQ(frame->scanline(1)[0], Gfx::Color(0, 255, 255).value());
    EXPECT_EQ(frame->scanline(1)[1], Gfx::Color(255, 0, 255).value());
    EXPECT_EQ(frame->scanline(1)[2], Gfx::Color(255, 255, 0).value());
}

TEST_CASE(test_jpeg2000_decode_unsupported)
{
    Array test_inputs = {
        TEST_INPUT("jpeg2000/kakadu-lossless-cmyka-u8-prog1-layers1-res6.jp2"sv),
        TEST_INPUT("jpeg2000/openjpeg-lossless-RGN.jp2"sv),
        TEST_INPUT("jpeg2000/openjpeg-lossless-bgra-u8.jp2"sv),
        TEST_INPUT("jpeg2000/openjpeg-lossless-rgba-u8-prog0-tile-part-index-overflow.jp2"sv),
        TEST_INPUT("jpeg2000/kakadu-lossless-lab-u8-prog1-layers1-res6.jp2"sv),
        TEST_INPUT("jpeg2000/kakadu-lossless-lab-alpha-u8-prog1-layers1-res6.jp2"sv),
    };

    for (auto test_input : test_inputs) {
        auto file = TRY_OR_FAIL(Core::MappedFile::map(test_input));
        EXPECT(Gfx::JPEG2000ImageDecoderPlugin::sniff(file->bytes()));
        auto plugin_decoder = TRY_OR_FAIL(Gfx::JPEG2000ImageDecoderPlugin::create(file->bytes()));
        auto frame = plugin_decoder->frame(0);
        EXPECT(frame.is_error());
    }
}

TEST_CASE(test_jpeg2000_icc)
{
    auto file = TRY_OR_FAIL(Core::MappedFile::map(TEST_INPUT("jpeg2000/kakadu-lossy-rgba-u8-prog0-layers1-res6-mct.jp2"sv)));
    EXPECT(Gfx::JPEG2000ImageDecoderPlugin::sniff(file->bytes()));
    auto plugin_decoder = TRY_OR_FAIL(Gfx::JPEG2000ImageDecoderPlugin::create(file->bytes()));

    auto icc_bytes = MUST(plugin_decoder->icc_data());
    EXPECT(icc_bytes.has_value());
    EXPECT_EQ(icc_bytes->size(), 3144u);
}

TEST_CASE(test_jpeg2000_decode_lossy)
{
    auto png_file = TRY_OR_FAIL(Core::MappedFile::map(TEST_INPUT("jpeg2000/ref.png"sv)));
    auto png_plugin_decoder = TRY_OR_FAIL(Gfx::PNGImageDecoderPlugin::create(png_file->bytes()));
    auto ref_frame = TRY_OR_FAIL(expect_single_frame_of_size(*png_plugin_decoder, { 119, 101 }));

    Array test_inputs = {
        TEST_INPUT("jpeg2000/kakadu-lossy-rgba-u8-prog0-layers1-res6-mct.jp2"sv),
        TEST_INPUT("jpeg2000/openjpeg-lossy-quantization-scalar-derived.jp2"sv),
    };

    for (auto test_input : test_inputs) {
        auto file = TRY_OR_FAIL(Core::MappedFile::map(test_input));
        EXPECT(Gfx::JPEG2000ImageDecoderPlugin::sniff(file->bytes()));
        auto plugin_decoder = TRY_OR_FAIL(Gfx::JPEG2000ImageDecoderPlugin::create(file->bytes()));
        auto frame = TRY_OR_FAIL(expect_single_frame_of_size(*plugin_decoder, { 119, 101 }));

        for (int y = 0; y < frame.image->height(); ++y) {
            for (int x = 0; x < frame.image->width(); ++x) {
                auto pixel = frame.image->get_pixel(x, y);
                auto ref_pixel = ref_frame.image->get_pixel(x, y);

                // FIXME: ref.png is kakadu-lossy-rgba-u8-prog0-layers1-res6-mct.jp2 opened in Photoshop and saved as png,
                // so the image data should be identical. Maybe lossy reconstruction isn't exact (maybe some decoders round
                // after every IDWT level and we don't, or something like this), but being off by 5 seems high.
                // Investigate and try to lower the threshold here, ideally probably to zero. If that happens, move the
                // decoding data checking part of this test to test_jpeg2000_decode.
                // (The lossy openjpeg file only needs a Threshold of 3 to pass.)
                constexpr int Threshold = 5;
                EXPECT(abs(pixel.red() - ref_pixel.red()) <= Threshold);
                EXPECT(abs(pixel.green() - ref_pixel.green()) <= Threshold);
                EXPECT(abs(pixel.blue() - ref_pixel.blue()) <= Threshold);
                EXPECT(abs(pixel.alpha() - ref_pixel.alpha()) <= Threshold);
            }
        }
    }
}

TEST_CASE(test_jpeg2000_gray)
{
    auto file = TRY_OR_FAIL(Core::MappedFile::map(TEST_INPUT("jpeg2000/buggie-gray.jpf"sv)));
    EXPECT(Gfx::JPEG2000ImageDecoderPlugin::sniff(file->bytes()));
    auto plugin_decoder = TRY_OR_FAIL(Gfx::JPEG2000ImageDecoderPlugin::create(file->bytes()));

    EXPECT_EQ(plugin_decoder->size(), Gfx::IntSize(64, 138));

    // The file contains both a simple and a real profile. Make sure we get the bigger one.
    auto icc_bytes = MUST(plugin_decoder->icc_data());
    EXPECT(icc_bytes.has_value());
    EXPECT_EQ(icc_bytes->size(), 912u);
}

TEST_CASE(test_jpeg2000_progression_iterators)
{
    {
        int const layer_count = 2;
        int const max_number_of_decomposition_levels = 2;
        int const component_count = 4;
        auto precinct_count = [](int, int) { return 5; };
        Gfx::JPEG2000::LayerResolutionLevelComponentPositionProgressionIterator iterator { layer_count, max_number_of_decomposition_levels, component_count, move(precinct_count) };

        for (int layer = 0; layer < layer_count; ++layer)
            for (int resolution_level = 0; resolution_level <= max_number_of_decomposition_levels; ++resolution_level)
                for (int component = 0; component < component_count; ++component)
                    for (int precinct = 0; precinct < precinct_count(resolution_level, component); ++precinct) {
                        EXPECT(iterator.has_next());
                        EXPECT_EQ(iterator.next(), (Gfx::JPEG2000::ProgressionData { .layer = layer, .resolution_level = resolution_level, .component = component, .precinct = precinct }));
                    }
        EXPECT(!iterator.has_next());
    }

    {
        int const layer_count = 2;
        int const max_number_of_decomposition_levels = 2;
        int const component_count = 4;
        auto precinct_count = [](int, int) { return 5; };
        Gfx::JPEG2000::ResolutionLevelLayerComponentPositionProgressionIterator iterator { layer_count, max_number_of_decomposition_levels, component_count, move(precinct_count) };

        for (int resolution_level = 0; resolution_level <= max_number_of_decomposition_levels; ++resolution_level)
            for (int layer = 0; layer < layer_count; ++layer)
                for (int component = 0; component < component_count; ++component)
                    for (int precinct = 0; precinct < precinct_count(resolution_level, component); ++precinct) {
                        EXPECT(iterator.has_next());
                        EXPECT_EQ(iterator.next(), (Gfx::JPEG2000::ProgressionData { .layer = layer, .resolution_level = resolution_level, .component = component, .precinct = precinct }));
                    }
        EXPECT(!iterator.has_next());
    }

    {
        int const layer_count = 2;
        int const max_number_of_decomposition_levels = 2;
        int const component_count = 4;
        int const precinct_count_number = 5;
        auto precinct_count = [](int, int) { return precinct_count_number; };

        Gfx::IntRect tile_rect { 0, 0, 5 * 32, 32 };
        auto XRsiz = [&](size_t) { return 1; };
        auto YRsiz = [&](size_t) { return 1; };

        auto PPx = [&](int r, int) { return 5 - (max_number_of_decomposition_levels - r); };
        auto PPy = [&](int r, int) { return 5 - (max_number_of_decomposition_levels - r); };
        auto N_L = [&](int) { return max_number_of_decomposition_levels; };
        auto num_precincts_wide = [&](int, int) { return precinct_count_number; };
        auto ll_rect = [&](int r, int) {
            return tile_rect / (1 << (max_number_of_decomposition_levels - r));
        };
        Gfx::JPEG2000::ResolutionLevelPositionComponentLayerProgressionIterator iterator {
            layer_count, max_number_of_decomposition_levels, component_count, move(precinct_count),
            move(XRsiz), move(YRsiz), move(PPx), move(PPy), move(N_L), move(num_precincts_wide), tile_rect, move(ll_rect)
        };

        for (int resolution_level = 0; resolution_level <= max_number_of_decomposition_levels; ++resolution_level)
            for (int precinct = 0; precinct < precinct_count_number; ++precinct)
                for (int component = 0; component < component_count; ++component)
                    for (int layer = 0; layer < layer_count; ++layer) {
                        EXPECT(iterator.has_next());
                        EXPECT_EQ(iterator.next(), (Gfx::JPEG2000::ProgressionData { .layer = layer, .resolution_level = resolution_level, .component = component, .precinct = precinct }));
                    }
        EXPECT(!iterator.has_next());
    }

    {
        int const layer_count = 2;
        int const max_number_of_decomposition_levels = 2;
        int const component_count = 4;
        int const precinct_count_number = 5;
        auto precinct_count = [](int, int) { return precinct_count_number; };

        Gfx::IntRect tile_rect { 0, 0, 5 * 32, 32 };
        auto XRsiz = [&](size_t) { return 1; };
        auto YRsiz = [&](size_t) { return 1; };

        auto PPx = [&](int r, int) { return 5 - (max_number_of_decomposition_levels - r); };
        auto PPy = [&](int r, int) { return 5 - (max_number_of_decomposition_levels - r); };
        auto N_L = [&](int) { return max_number_of_decomposition_levels; };
        auto num_precincts_wide = [&](int, int) { return precinct_count_number; };
        auto ll_rect = [&](int r, int) {
            return tile_rect / (1 << (max_number_of_decomposition_levels - r));
        };
        Gfx::JPEG2000::PositionComponentResolutionLevelLayerProgressionIterator iterator {
            layer_count, component_count, move(precinct_count),
            move(XRsiz), move(YRsiz), move(PPx), move(PPy), move(N_L), move(num_precincts_wide), tile_rect, move(ll_rect)
        };

        for (int precinct = 0; precinct < precinct_count_number; ++precinct)
            for (int component = 0; component < component_count; ++component)
                for (int resolution_level = 0; resolution_level <= max_number_of_decomposition_levels; ++resolution_level)
                    for (int layer = 0; layer < layer_count; ++layer) {
                        EXPECT(iterator.has_next());
                        EXPECT_EQ(iterator.next(), (Gfx::JPEG2000::ProgressionData { .layer = layer, .resolution_level = resolution_level, .component = component, .precinct = precinct }));
                    }
        EXPECT(!iterator.has_next());
    }

    {
        int const layer_count = 2;
        int const max_number_of_decomposition_levels = 2;
        int const component_count = 4;
        int const precinct_count_number = 5;
        auto precinct_count = [](int, int) { return precinct_count_number; };

        Gfx::IntRect tile_rect { 0, 0, 5 * 32, 32 };
        auto XRsiz = [&](size_t) { return 1; };
        auto YRsiz = [&](size_t) { return 1; };

        auto PPx = [&](int r, int) { return 5 - (max_number_of_decomposition_levels - r); };
        auto PPy = [&](int r, int) { return 5 - (max_number_of_decomposition_levels - r); };
        auto N_L = [&](int) { return max_number_of_decomposition_levels; };
        auto num_precincts_wide = [&](int, int) { return precinct_count_number; };
        auto ll_rect = [&](int r, int) {
            return tile_rect / (1 << (max_number_of_decomposition_levels - r));
        };
        Gfx::JPEG2000::ComponentPositionResolutionLevelLayerProgressionIterator iterator {
            layer_count, component_count, move(precinct_count),
            move(XRsiz), move(YRsiz), move(PPx), move(PPy), move(N_L), move(num_precincts_wide), tile_rect, move(ll_rect)
        };

        for (int component = 0; component < component_count; ++component)
            for (int precinct = 0; precinct < precinct_count_number; ++precinct)
                for (int resolution_level = 0; resolution_level <= max_number_of_decomposition_levels; ++resolution_level)
                    for (int layer = 0; layer < layer_count; ++layer) {
                        EXPECT(iterator.has_next());
                        EXPECT_EQ(iterator.next(), (Gfx::JPEG2000::ProgressionData { .layer = layer, .resolution_level = resolution_level, .component = component, .precinct = precinct }));
                    }
        EXPECT(!iterator.has_next());
    }
}

TEST_CASE(test_jpeg2000_tag_tree)
{
    {
        // The example from the NOTE at the end of B.10.2 Tag trees:
        auto tree = TRY_OR_FAIL(Gfx::JPEG2000::TagTree::create(6, 3));
        auto bits = to_array<u8>({
            0, 1, 1, 1, 1, // q3(0, 0)
            0, 0, 1,       // q3(1, 0)
            1, 0, 1,       // q3(2, 0)
        });
        size_t index = 0;
        Function<ErrorOr<bool>()> read_bit = [&]() -> bool {
            return bits[index++];
        };
        EXPECT_EQ(1u, MUST(tree.read_value(0, 0, read_bit)));
        EXPECT_EQ(index, 5u);
        EXPECT_EQ(3u, MUST(tree.read_value(1, 0, read_bit)));
        EXPECT_EQ(index, 8u);
        EXPECT_EQ(2u, MUST(tree.read_value(2, 0, read_bit)));
        EXPECT_EQ(index, 11u);
    }

    {
        // The inclusion tag tree bits from Table B.5 – Example packet header bit stream.
        auto tree = TRY_OR_FAIL(Gfx::JPEG2000::TagTree::create(3, 2));
        auto bits = to_array<u8>({
            1, 1, 1, // Code-block 0, 0 included for the first time (partial inclusion tag tree)
            1,       // Code-block 1, 0 included for the first time (partial inclusion tag tree)
            0,       // Code-block 2, 0 not yet included (partial tag tree)
            0,       // Code-block 0, 1 not yet included
            0,       // Code-block 1, 2 not yet included
            // Code-block 2, 1 not yet included (no data needed, already conveyed by partial tag tree for code-block 2, 0)
        });
        size_t index = 0;
        Function<ErrorOr<bool>()> read_bit = [&]() -> bool {
            return bits[index++];
        };
        u32 next_layer = 1;
        EXPECT_EQ(0u, MUST(tree.read_value(0, 0, read_bit, next_layer)));
        EXPECT_EQ(index, 3u);
        EXPECT_EQ(0u, MUST(tree.read_value(1, 0, read_bit, next_layer)));
        EXPECT_EQ(index, 4u);
        EXPECT_EQ(1u, MUST(tree.read_value(2, 0, read_bit, next_layer)));
        EXPECT_EQ(index, 5u);
        EXPECT_EQ(1u, MUST(tree.read_value(0, 1, read_bit, next_layer)));
        EXPECT_EQ(index, 6u);
        EXPECT_EQ(1u, MUST(tree.read_value(1, 1, read_bit, next_layer)));
        EXPECT_EQ(index, 7u);
        EXPECT_EQ(1u, MUST(tree.read_value(2, 1, read_bit, next_layer)));
        EXPECT_EQ(index, 7u); // Didn't change!
    }

    {
        // This isn't in the spec. If one dimension is 2^n + 1 and the other side is just 1, then the topmost node will have
        // 2^n x 1 and 1 x 1 children. The first child will have n levels of children. The 1 x 1 child could end immediately,
        // or it could require that it also has n levels of (all 1 x 1) children. The spec isn't clear on which of
        // the two alternatives should happen. We currently have n levels of 1 x 1 blocks.
        constexpr auto n = 5;
        auto tree = TRY_OR_FAIL(Gfx::JPEG2000::TagTree::create((1 << n) + 1, 1));
        Vector<u8> bits;
        bits.append(1); // Finalize topmost node.
        bits.append(0); // Increment value in 1 x 1 child.
        bits.append(1); // Finalize 1 x 1 child.

        // Finalize further 1 x 1 children, if present.
        for (size_t i = 0; i < n; ++i)
            bits.append(1);

        size_t index = 0;
        Function<ErrorOr<bool>()> read_bit = [&]() -> bool {
            return bits[index++];
        };

        EXPECT_EQ(1u, MUST(tree.read_value(1 << n, 0, read_bit)));

        // This will read either 3 or 3 + n bits, depending on the interpretation.
        EXPECT_EQ(index, 3u + n);
    }
}

TEST_CASE(test_pam_rgb)
{
    auto file = TRY_OR_FAIL(Core::MappedFile::map(TEST_INPUT("pnm/2x1.pam"sv)));
    EXPECT(Gfx::PAMImageDecoderPlugin::sniff(file->bytes()));
    auto plugin_decoder = TRY_OR_FAIL(Gfx::PAMImageDecoderPlugin::create(file->bytes()));

    auto frame = TRY_OR_FAIL(expect_single_frame(*plugin_decoder));
    EXPECT_EQ(frame.image->size(), Gfx::IntSize(2, 1));
    EXPECT_EQ(frame.image->get_pixel(0, 0), Gfx::Color('0', 'z', '0'));
    EXPECT_EQ(frame.image->get_pixel(1, 0), Gfx::Color('0', '0', 'z'));
}

TEST_CASE(test_pam_cmyk)
{
    auto file = TRY_OR_FAIL(Core::MappedFile::map(TEST_INPUT("pnm/2x1-cmyk.pam"sv)));
    EXPECT(Gfx::PAMImageDecoderPlugin::sniff(file->bytes()));
    auto plugin_decoder = TRY_OR_FAIL(Gfx::PAMImageDecoderPlugin::create(file->bytes()));

    EXPECT_EQ(plugin_decoder->natural_frame_format(), Gfx::NaturalFrameFormat::CMYK);
    auto cmyk_frame = TRY_OR_FAIL(plugin_decoder->cmyk_frame());
    EXPECT_EQ(cmyk_frame->size(), Gfx::IntSize(2, 1));
    EXPECT_EQ(cmyk_frame->begin()[0], (Gfx::CMYK { '0', 'z', '0', 'y' }));
    EXPECT_EQ(cmyk_frame->begin()[1], (Gfx::CMYK { '0', '0', 'z', 'y' }));

    auto frame = TRY_OR_FAIL(expect_single_frame(*plugin_decoder));
    EXPECT_EQ(frame.image->size(), Gfx::IntSize(2, 1));
    EXPECT_EQ(frame.image->get_pixel(0, 0), Gfx::Color('l', 'E', 'l'));
    EXPECT_EQ(frame.image->get_pixel(1, 0), Gfx::Color('l', 'l', 'E'));
}

TEST_CASE(test_pbm)
{
    auto file = TRY_OR_FAIL(Core::MappedFile::map(TEST_INPUT("pnm/buggie-raw.pbm"sv)));
    EXPECT(Gfx::PBMImageDecoderPlugin::sniff(file->bytes()));
    auto plugin_decoder = TRY_OR_FAIL(Gfx::PBMImageDecoderPlugin::create(file->bytes()));

    TRY_OR_FAIL(expect_single_frame(*plugin_decoder));
}

TEST_CASE(test_pgm)
{
    auto file = TRY_OR_FAIL(Core::MappedFile::map(TEST_INPUT("pnm/buggie-raw.pgm"sv)));
    EXPECT(Gfx::PGMImageDecoderPlugin::sniff(file->bytes()));
    auto plugin_decoder = TRY_OR_FAIL(Gfx::PGMImageDecoderPlugin::create(file->bytes()));

    TRY_OR_FAIL(expect_single_frame(*plugin_decoder));
}

TEST_CASE(test_png)
{
    auto file = TRY_OR_FAIL(Core::MappedFile::map(TEST_INPUT("png/buggie.png"sv)));
    EXPECT(Gfx::PNGImageDecoderPlugin::sniff(file->bytes()));
    auto plugin_decoder = TRY_OR_FAIL(Gfx::PNGImageDecoderPlugin::create(file->bytes()));

    TRY_OR_FAIL(expect_single_frame(*plugin_decoder));
}

TEST_CASE(test_exif)
{
    auto file = TRY_OR_FAIL(Core::MappedFile::map(TEST_INPUT("png/exif.png"sv)));
    EXPECT(Gfx::PNGImageDecoderPlugin::sniff(file->bytes()));
    auto plugin_decoder = TRY_OR_FAIL(Gfx::PNGImageDecoderPlugin::create(file->bytes()));

    TRY_OR_FAIL(expect_single_frame_of_size(*plugin_decoder, { 100, 200 }));
    EXPECT(plugin_decoder->metadata().has_value());
    auto const& exif_metadata = static_cast<Gfx::ExifMetadata const&>(plugin_decoder->metadata().value());
    EXPECT_EQ(*exif_metadata.orientation(), Gfx::TIFF::Orientation::Rotate90Clockwise);
}

TEST_CASE(test_png_malformed_frame)
{
    Array test_inputs = {
        TEST_INPUT("png/oss-fuzz-testcase-62371.png"sv),
        TEST_INPUT("png/oss-fuzz-testcase-63052.png"sv)
    };

    for (auto test_input : test_inputs) {
        auto file = TRY_OR_FAIL(Core::MappedFile::map(test_input));
        auto plugin_decoder = TRY_OR_FAIL(Gfx::PNGImageDecoderPlugin::create(file->bytes()));
        auto frame_or_error = plugin_decoder->frame(0);
        EXPECT(frame_or_error.is_error());
    }
}

TEST_CASE(test_ppm)
{
    auto file = TRY_OR_FAIL(Core::MappedFile::map(TEST_INPUT("pnm/buggie-raw.ppm"sv)));
    EXPECT(Gfx::PPMImageDecoderPlugin::sniff(file->bytes()));
    auto plugin_decoder = TRY_OR_FAIL(Gfx::PPMImageDecoderPlugin::create(file->bytes()));

    TRY_OR_FAIL(expect_single_frame(*plugin_decoder));
}

TEST_CASE(test_targa_bottom_left)
{
    auto file = TRY_OR_FAIL(Core::MappedFile::map(TEST_INPUT("tga/buggie-bottom-left-uncompressed.tga"sv)));
    EXPECT(Gfx::TGAImageDecoderPlugin::validate_before_create(file->bytes()));
    auto plugin_decoder = TRY_OR_FAIL(Gfx::TGAImageDecoderPlugin::create(file->bytes()));

    TRY_OR_FAIL(expect_single_frame(*plugin_decoder));
}

TEST_CASE(test_targa_top_left)
{
    auto file = TRY_OR_FAIL(Core::MappedFile::map(TEST_INPUT("tga/buggie-top-left-uncompressed.tga"sv)));
    EXPECT(Gfx::TGAImageDecoderPlugin::validate_before_create(file->bytes()));
    auto plugin_decoder = TRY_OR_FAIL(Gfx::TGAImageDecoderPlugin::create(file->bytes()));

    TRY_OR_FAIL(expect_single_frame(*plugin_decoder));
}

TEST_CASE(test_targa_bottom_left_compressed)
{
    auto file = TRY_OR_FAIL(Core::MappedFile::map(TEST_INPUT("tga/buggie-bottom-left-compressed.tga"sv)));
    EXPECT(Gfx::TGAImageDecoderPlugin::validate_before_create(file->bytes()));
    auto plugin_decoder = TRY_OR_FAIL(Gfx::TGAImageDecoderPlugin::create(file->bytes()));

    TRY_OR_FAIL(expect_single_frame(*plugin_decoder));
}

TEST_CASE(test_targa_top_left_compressed)
{
    auto file = TRY_OR_FAIL(Core::MappedFile::map(TEST_INPUT("tga/buggie-top-left-compressed.tga"sv)));
    EXPECT(Gfx::TGAImageDecoderPlugin::validate_before_create(file->bytes()));
    auto plugin_decoder = TRY_OR_FAIL(Gfx::TGAImageDecoderPlugin::create(file->bytes()));

    TRY_OR_FAIL(expect_single_frame(*plugin_decoder));
}

TEST_CASE(test_targa_black_and_white_uncompressed)
{
    auto file = TRY_OR_FAIL(Core::MappedFile::map(TEST_INPUT("tga/buggie-black-and-white-uncompressed.tga"sv)));
    EXPECT(Gfx::TGAImageDecoderPlugin::validate_before_create(file->bytes()));
    auto plugin_decoder = TRY_OR_FAIL(Gfx::TGAImageDecoderPlugin::create(file->bytes()));

    TRY_OR_FAIL(expect_single_frame(*plugin_decoder));
}

TEST_CASE(test_targa_image_descriptor)
{
    Array test_inputs = {
        TEST_INPUT("tga/square-bottom-left.tga"sv),
        TEST_INPUT("tga/square-bottom-right.tga"sv),
        TEST_INPUT("tga/square-top-left.tga"sv),
        TEST_INPUT("tga/square-top-right.tga"sv),
    };

    for (auto test_input : test_inputs) {
        auto file = TRY_OR_FAIL(Core::MappedFile::map(test_input));
        EXPECT(Gfx::TGAImageDecoderPlugin::validate_before_create(file->bytes()));
        auto plugin_decoder = TRY_OR_FAIL(Gfx::TGAImageDecoderPlugin::create(file->bytes()));

        auto frame = TRY_OR_FAIL(expect_single_frame_of_size(*plugin_decoder, { 2, 2 }));

        EXPECT_EQ(frame.image->get_pixel(0, 0), Gfx::Color::NamedColor::Red);
        EXPECT_EQ(frame.image->get_pixel(1, 0), Gfx::Color::NamedColor::Green);
        EXPECT_EQ(frame.image->get_pixel(0, 1), Gfx::Color::NamedColor::Blue);
        EXPECT_EQ(frame.image->get_pixel(1, 1), Gfx::Color::NamedColor::Magenta);
    }
}

TEST_CASE(test_targa_image_with_identifier)
{
    auto file = TRY_OR_FAIL(Core::MappedFile::map(TEST_INPUT("tga/image-with-identifier.tga"sv)));
    EXPECT(Gfx::TGAImageDecoderPlugin::validate_before_create(file->bytes()));
    auto plugin_decoder = TRY_OR_FAIL(Gfx::TGAImageDecoderPlugin::create(file->bytes()));

    auto metadata = plugin_decoder->metadata();
    EXPECT(metadata.has_value());

    auto& tags = metadata->main_tags();
    EXPECT_EQ(tags.size(), 1U);
    auto ident = tags.get("Identifier"sv);
    EXPECT(ident.has_value());
    EXPECT_EQ(*ident, "Hello, TGA!"sv);

    auto frame = TRY_OR_FAIL(expect_single_frame_of_size(*plugin_decoder, { 24, 24 }));
    EXPECT_EQ(frame.image->get_pixel(5, 5), Gfx::Color(40, 172, 255));
}

TEST_CASE(test_tiff_uncompressed)
{
    auto file = TRY_OR_FAIL(Core::MappedFile::map(TEST_INPUT("tiff/uncompressed.tiff"sv)));
    EXPECT(Gfx::TIFFImageDecoderPlugin::sniff(file->bytes()));
    auto plugin_decoder = TRY_OR_FAIL(Gfx::TIFFImageDecoderPlugin::create(file->bytes()));

    auto frame = TRY_OR_FAIL(expect_single_frame_of_size(*plugin_decoder, { 400, 300 }));

    EXPECT_EQ(frame.image->get_pixel(0, 0), Gfx::Color::NamedColor::White);
    EXPECT_EQ(frame.image->get_pixel(60, 75), Gfx::Color::NamedColor::Red);
}

TEST_CASE(test_tiff_ccitt_rle)
{
    auto file = TRY_OR_FAIL(Core::MappedFile::map(TEST_INPUT("tiff/ccitt_rle.tiff"sv)));
    EXPECT(Gfx::TIFFImageDecoderPlugin::sniff(file->bytes()));
    auto plugin_decoder = TRY_OR_FAIL(Gfx::TIFFImageDecoderPlugin::create(file->bytes()));

    auto frame = TRY_OR_FAIL(expect_single_frame_of_size(*plugin_decoder, { 400, 300 }));

    EXPECT_EQ(frame.image->get_pixel(0, 0), Gfx::Color::NamedColor::White);
    EXPECT_EQ(frame.image->get_pixel(60, 75), Gfx::Color::NamedColor::Black);
}

TEST_CASE(test_tiff_ccitt3)
{
    auto file = TRY_OR_FAIL(Core::MappedFile::map(TEST_INPUT("tiff/ccitt3.tiff"sv)));
    EXPECT(Gfx::TIFFImageDecoderPlugin::sniff(file->bytes()));
    auto plugin_decoder = TRY_OR_FAIL(Gfx::TIFFImageDecoderPlugin::create(file->bytes()));

    auto frame = TRY_OR_FAIL(expect_single_frame_of_size(*plugin_decoder, { 400, 300 }));

    EXPECT_EQ(frame.image->get_pixel(0, 0), Gfx::Color::NamedColor::White);
    EXPECT_EQ(frame.image->get_pixel(60, 75), Gfx::Color::NamedColor::Black);
}

TEST_CASE(test_tiff_ccitt3_no_tags)
{
    auto file = TRY_OR_FAIL(Core::MappedFile::map(TEST_INPUT("tiff/ccitt3_no_tags.tiff"sv)));
    EXPECT(Gfx::TIFFImageDecoderPlugin::sniff(file->bytes()));
    auto plugin_decoder = TRY_OR_FAIL(Gfx::TIFFImageDecoderPlugin::create(file->bytes()));

    auto frame = TRY_OR_FAIL(expect_single_frame_of_size(*plugin_decoder, { 6, 4 }));

    EXPECT_EQ(frame.image->get_pixel(0, 0), Gfx::Color::NamedColor::White);
    EXPECT_EQ(frame.image->get_pixel(3, 0), Gfx::Color::NamedColor::Black);
    EXPECT_EQ(frame.image->get_pixel(2, 2), Gfx::Color::NamedColor::White);
    EXPECT_EQ(frame.image->get_pixel(5, 3), Gfx::Color::NamedColor::White);
}

TEST_CASE(test_tiff_ccitt3_fill)
{
    auto file = TRY_OR_FAIL(Core::MappedFile::map(TEST_INPUT("tiff/ccitt3_1d_fill.tiff"sv)));
    EXPECT(Gfx::TIFFImageDecoderPlugin::sniff(file->bytes()));
    auto plugin_decoder = TRY_OR_FAIL(Gfx::TIFFImageDecoderPlugin::create(file->bytes()));

    auto frame = TRY_OR_FAIL(expect_single_frame_of_size(*plugin_decoder, { 6, 4 }));

    EXPECT_EQ(frame.image->get_pixel(0, 0), Gfx::Color::NamedColor::White);
    EXPECT_EQ(frame.image->get_pixel(3, 0), Gfx::Color::NamedColor::Black);
    EXPECT_EQ(frame.image->get_pixel(2, 2), Gfx::Color::NamedColor::White);
    EXPECT_EQ(frame.image->get_pixel(5, 3), Gfx::Color::NamedColor::White);
}

TEST_CASE(test_tiff_ccitt3_2d)
{
    auto file = TRY_OR_FAIL(Core::MappedFile::map(TEST_INPUT("tiff/ccitt3_2d.tiff"sv)));
    EXPECT(Gfx::TIFFImageDecoderPlugin::sniff(file->bytes()));
    auto plugin_decoder = TRY_OR_FAIL(Gfx::TIFFImageDecoderPlugin::create(file->bytes()));

    auto frame = TRY_OR_FAIL(expect_single_frame_of_size(*plugin_decoder, { 400, 300 }));

    EXPECT_EQ(frame.image->get_pixel(0, 0), Gfx::Color::NamedColor::White);
    EXPECT_EQ(frame.image->get_pixel(60, 75), Gfx::Color::NamedColor::Black);
}

TEST_CASE(test_tiff_ccitt3_2d_fill)
{
    auto file = TRY_OR_FAIL(Core::MappedFile::map(TEST_INPUT("tiff/ccitt3_2d_fill.tiff"sv)));
    EXPECT(Gfx::TIFFImageDecoderPlugin::sniff(file->bytes()));
    auto plugin_decoder = TRY_OR_FAIL(Gfx::TIFFImageDecoderPlugin::create(file->bytes()));

    auto frame = TRY_OR_FAIL(expect_single_frame_of_size(*plugin_decoder, { 400, 300 }));

    EXPECT_EQ(frame.image->get_pixel(0, 0), Gfx::Color::NamedColor::White);
    EXPECT_EQ(frame.image->get_pixel(60, 75), Gfx::Color::NamedColor::Black);
}

TEST_CASE(test_tiff_ccitt4)
{
    auto file = TRY_OR_FAIL(Core::MappedFile::map(TEST_INPUT("tiff/ccitt4.tiff"sv)));
    EXPECT(Gfx::TIFFImageDecoderPlugin::sniff(file->bytes()));
    auto plugin_decoder = TRY_OR_FAIL(Gfx::TIFFImageDecoderPlugin::create(file->bytes()));

    auto frame = TRY_OR_FAIL(expect_single_frame_of_size(*plugin_decoder, { 400, 300 }));

    EXPECT_EQ(frame.image->get_pixel(0, 0), Gfx::Color::NamedColor::White);
    EXPECT_EQ(frame.image->get_pixel(60, 75), Gfx::Color::NamedColor::Black);
}

TEST_CASE(test_tiff_lzw)
{
    auto file = TRY_OR_FAIL(Core::MappedFile::map(TEST_INPUT("tiff/lzw.tiff"sv)));
    EXPECT(Gfx::TIFFImageDecoderPlugin::sniff(file->bytes()));
    auto plugin_decoder = TRY_OR_FAIL(Gfx::TIFFImageDecoderPlugin::create(file->bytes()));

    auto frame = TRY_OR_FAIL(expect_single_frame_of_size(*plugin_decoder, { 400, 300 }));

    EXPECT_EQ(frame.image->get_pixel(0, 0), Gfx::Color::NamedColor::White);
    EXPECT_EQ(frame.image->get_pixel(60, 75), Gfx::Color::NamedColor::Red);
}

TEST_CASE(test_tiff_deflate)
{
    auto file = TRY_OR_FAIL(Core::MappedFile::map(TEST_INPUT("tiff/deflate.tiff"sv)));
    EXPECT(Gfx::TIFFImageDecoderPlugin::sniff(file->bytes()));
    auto plugin_decoder = TRY_OR_FAIL(Gfx::TIFFImageDecoderPlugin::create(file->bytes()));

    auto frame = TRY_OR_FAIL(expect_single_frame_of_size(*plugin_decoder, { 400, 300 }));

    EXPECT_EQ(frame.image->get_pixel(0, 0), Gfx::Color::NamedColor::White);
    EXPECT_EQ(frame.image->get_pixel(60, 75), Gfx::Color::NamedColor::Red);
}

TEST_CASE(test_tiff_krita)
{
    auto file = TRY_OR_FAIL(Core::MappedFile::map(TEST_INPUT("tiff/krita.tif"sv)));
    EXPECT(Gfx::TIFFImageDecoderPlugin::sniff(file->bytes()));
    auto plugin_decoder = TRY_OR_FAIL(Gfx::TIFFImageDecoderPlugin::create(file->bytes()));

    auto frame = TRY_OR_FAIL(expect_single_frame_of_size(*plugin_decoder, { 400, 300 }));

    EXPECT_EQ(frame.image->get_pixel(0, 0), Gfx::Color::NamedColor::White);
    EXPECT_EQ(frame.image->get_pixel(60, 75), Gfx::Color::NamedColor::Red);
}

TEST_CASE(test_tiff_orientation)
{
    auto file = TRY_OR_FAIL(Core::MappedFile::map(TEST_INPUT("tiff/orientation.tiff"sv)));
    EXPECT(Gfx::TIFFImageDecoderPlugin::sniff(file->bytes()));
    auto plugin_decoder = TRY_OR_FAIL(Gfx::TIFFImageDecoderPlugin::create(file->bytes()));

    auto frame = TRY_OR_FAIL(expect_single_frame_of_size(*plugin_decoder, { 300, 400 }));

    // Orientation is Rotate90Clockwise
    EXPECT_EQ(frame.image->get_pixel(0, 0), Gfx::Color::NamedColor::White);
    EXPECT_EQ(frame.image->get_pixel(300 - 75, 60), Gfx::Color::NamedColor::Red);
}

TEST_CASE(test_tiff_packed_bits)
{
    auto file = TRY_OR_FAIL(Core::MappedFile::map(TEST_INPUT("tiff/packed_bits.tiff"sv)));
    EXPECT(Gfx::TIFFImageDecoderPlugin::sniff(file->bytes()));
    auto plugin_decoder = TRY_OR_FAIL(Gfx::TIFFImageDecoderPlugin::create(file->bytes()));

    auto frame = TRY_OR_FAIL(expect_single_frame_of_size(*plugin_decoder, { 400, 300 }));

    EXPECT_EQ(frame.image->get_pixel(0, 0), Gfx::Color::NamedColor::White);
    EXPECT_EQ(frame.image->get_pixel(60, 75), Gfx::Color::NamedColor::Red);
}

TEST_CASE(test_tiff_grayscale)
{
    auto file = TRY_OR_FAIL(Core::MappedFile::map(TEST_INPUT("tiff/grayscale.tiff"sv)));
    EXPECT(Gfx::TIFFImageDecoderPlugin::sniff(file->bytes()));
    auto plugin_decoder = TRY_OR_FAIL(Gfx::TIFFImageDecoderPlugin::create(file->bytes()));

    auto frame = TRY_OR_FAIL(expect_single_frame_of_size(*plugin_decoder, { 400, 300 }));

    EXPECT_EQ(frame.image->get_pixel(0, 0), Gfx::Color::NamedColor::White);
    EXPECT_EQ(frame.image->get_pixel(60, 75), Gfx::Color(130, 130, 130));
}

TEST_CASE(test_tiff_grayscale_alpha)
{
    auto file = TRY_OR_FAIL(Core::MappedFile::map(TEST_INPUT("tiff/grayscale_alpha.tiff"sv)));
    EXPECT(Gfx::TIFFImageDecoderPlugin::sniff(file->bytes()));
    auto plugin_decoder = TRY_OR_FAIL(Gfx::TIFFImageDecoderPlugin::create(file->bytes()));

    auto frame = TRY_OR_FAIL(expect_single_frame_of_size(*plugin_decoder, { 400, 300 }));

    EXPECT_EQ(frame.image->get_pixel(0, 0).alpha(), 0);
    EXPECT_EQ(frame.image->get_pixel(60, 75), Gfx::Color(130, 130, 130));
}

TEST_CASE(test_tiff_rgb_alpha)
{
    auto file = TRY_OR_FAIL(Core::MappedFile::map(TEST_INPUT("tiff/rgb_alpha.tiff"sv)));
    EXPECT(Gfx::TIFFImageDecoderPlugin::sniff(file->bytes()));
    auto plugin_decoder = TRY_OR_FAIL(Gfx::TIFFImageDecoderPlugin::create(file->bytes()));

    auto frame = TRY_OR_FAIL(expect_single_frame_of_size(*plugin_decoder, { 400, 300 }));

    EXPECT_EQ(frame.image->get_pixel(0, 0).alpha(), 0);
    EXPECT_EQ(frame.image->get_pixel(60, 75), Gfx::Color::NamedColor::Red);
}

TEST_CASE(test_tiff_palette_alpha)
{
    auto file = TRY_OR_FAIL(Core::MappedFile::map(TEST_INPUT("tiff/rgb_palette_alpha.tiff"sv)));
    EXPECT(Gfx::TIFFImageDecoderPlugin::sniff(file->bytes()));
    auto plugin_decoder = TRY_OR_FAIL(Gfx::TIFFImageDecoderPlugin::create(file->bytes()));

    auto frame = TRY_OR_FAIL(expect_single_frame_of_size(*plugin_decoder, { 400, 300 }));

    EXPECT_EQ(frame.image->get_pixel(0, 0).alpha(), 0);
    EXPECT_EQ(frame.image->get_pixel(60, 75), Gfx::Color::NamedColor::Red);
}

TEST_CASE(test_tiff_alpha_predictor)
{
    auto file = TRY_OR_FAIL(Core::MappedFile::map(TEST_INPUT("tiff/alpha_predictor.tiff"sv)));
    EXPECT(Gfx::TIFFImageDecoderPlugin::sniff(file->bytes()));
    auto plugin_decoder = TRY_OR_FAIL(Gfx::TIFFImageDecoderPlugin::create(file->bytes()));

    auto frame = TRY_OR_FAIL(expect_single_frame_of_size(*plugin_decoder, { 400, 300 }));

    EXPECT_EQ(frame.image->get_pixel(0, 0).alpha(), 255);
    EXPECT_EQ(frame.image->get_pixel(60, 75), Gfx::Color::NamedColor::Red);
}

TEST_CASE(test_tiff_16_bits)
{
    auto file = TRY_OR_FAIL(Core::MappedFile::map(TEST_INPUT("tiff/16_bits.tiff"sv)));
    EXPECT(Gfx::TIFFImageDecoderPlugin::sniff(file->bytes()));
    auto plugin_decoder = TRY_OR_FAIL(Gfx::TIFFImageDecoderPlugin::create(file->bytes()));

    auto frame = TRY_OR_FAIL(expect_single_frame_of_size(*plugin_decoder, { 400, 300 }));

    EXPECT_EQ(frame.image->get_pixel(0, 0), Gfx::Color::NamedColor::White);
    EXPECT_EQ(frame.image->get_pixel(60, 75), Gfx::Color::NamedColor::Red);
}

TEST_CASE(test_tiff_cmyk)
{
    auto file = TRY_OR_FAIL(Core::MappedFile::map(TEST_INPUT("tiff/cmyk.tiff"sv)));
    EXPECT(Gfx::TIFFImageDecoderPlugin::sniff(file->bytes()));
    auto plugin_decoder = TRY_OR_FAIL(Gfx::TIFFImageDecoderPlugin::create(file->bytes()));

    auto frame = TRY_OR_FAIL(expect_single_frame_of_size(*plugin_decoder, { 400, 300 }));

    EXPECT_EQ(frame.image->get_pixel(0, 0), Gfx::Color::NamedColor::White);
    // I stripped the ICC profile from the image, so we can't test for equality with Red here.
    EXPECT_NE(frame.image->get_pixel(60, 75), Gfx::Color::NamedColor::White);
}

TEST_CASE(test_tiff_cmyk_raw)
{
    auto file = TRY_OR_FAIL(Core::MappedFile::map(TEST_INPUT("tiff/cmyk-small.tif"sv)));
    EXPECT(Gfx::TIFFImageDecoderPlugin::sniff(file->bytes()));
    auto plugin_decoder = TRY_OR_FAIL(Gfx::TIFFImageDecoderPlugin::create(file->bytes()));

    EXPECT_EQ(plugin_decoder->natural_frame_format(), Gfx::NaturalFrameFormat::CMYK);
    auto cmyk_frame = TRY_OR_FAIL(plugin_decoder->cmyk_frame());
    EXPECT_EQ(cmyk_frame->size(), Gfx::IntSize(2, 3));
    EXPECT_EQ(cmyk_frame->scanline(0)[0], (Gfx::CMYK { 0, 0, 0, 0 }));
    EXPECT_EQ(cmyk_frame->scanline(0)[1], (Gfx::CMYK { 0, 0, 0, 255 }));
    EXPECT_EQ(cmyk_frame->scanline(1)[0], (Gfx::CMYK { 255, 0, 0, 0 }));
    EXPECT_EQ(cmyk_frame->scanline(1)[1], (Gfx::CMYK { 0, 255, 0, 0 }));
    EXPECT_EQ(cmyk_frame->scanline(2)[0], (Gfx::CMYK { 0, 0, 255, 0 }));
    EXPECT_EQ(cmyk_frame->scanline(2)[1], (Gfx::CMYK { 255, 255, 255, 0 }));
}

TEST_CASE(test_tiff_tiled)
{
    auto file = TRY_OR_FAIL(Core::MappedFile::map(TEST_INPUT("tiff/tiled.tiff"sv)));
    EXPECT(Gfx::TIFFImageDecoderPlugin::sniff(file->bytes()));
    auto plugin_decoder = TRY_OR_FAIL(Gfx::TIFFImageDecoderPlugin::create(file->bytes()));

    auto frame = TRY_OR_FAIL(expect_single_frame_of_size(*plugin_decoder, { 400, 300 }));

    EXPECT_EQ(frame.image->get_pixel(0, 0), Gfx::Color::NamedColor::White);
    EXPECT_EQ(frame.image->get_pixel(60, 75), Gfx::Color::NamedColor::Red);
}

TEST_CASE(test_tiff_invalid_tag)
{
    auto file = TRY_OR_FAIL(Core::MappedFile::map(TEST_INPUT("tiff/invalid_tag.tiff"sv)));
    EXPECT(Gfx::TIFFImageDecoderPlugin::sniff(file->bytes()));
    auto plugin_decoder = TRY_OR_FAIL(Gfx::TIFFImageDecoderPlugin::create(file->bytes()));

    auto frame = TRY_OR_FAIL(expect_single_frame_of_size(*plugin_decoder, { 10, 10 }));

    EXPECT_EQ(frame.image->get_pixel(0, 0), Gfx::Color::NamedColor::Black);
    EXPECT_EQ(frame.image->get_pixel(0, 9), Gfx::Color::NamedColor::White);
}

TEST_CASE(test_tiff_fuzz)
{
    Array test_inputs = {
        TEST_INPUT("tiff/fuzz/clusterfuzz-testcase-minimized-FuzzTIFFLoader-5992525535969280"sv),
        TEST_INPUT("tiff/fuzz/clusterfuzz-testcase-minimized-FuzzTIFFLoader-4568002606465024"sv),
        TEST_INPUT("tiff/fuzz/clusterfuzz-testcase-minimized-FuzzTIFFLoader-4568095444238336"sv),
        TEST_INPUT("tiff/fuzz/clusterfuzz-testcase-minimized-FuzzTIFFLoader-4792178760024064"sv),
        TEST_INPUT("tiff/fuzz/clusterfuzz-testcase-minimized-FuzzTIFFLoader-4843377133682688"sv),
        TEST_INPUT("tiff/fuzz/clusterfuzz-testcase-minimized-FuzzTIFFLoader-5141058625273856"sv),
        TEST_INPUT("tiff/fuzz/clusterfuzz-testcase-minimized-FuzzTIFFLoader-5148654958608384"sv),
        TEST_INPUT("tiff/fuzz/clusterfuzz-testcase-minimized-FuzzTIFFLoader-5153321679650816"sv),
        TEST_INPUT("tiff/fuzz/clusterfuzz-testcase-minimized-FuzzTIFFLoader-5185775050227712"sv),
        TEST_INPUT("tiff/fuzz/clusterfuzz-testcase-minimized-FuzzTIFFLoader-5355241142812672.fuzz"sv),
        TEST_INPUT("tiff/fuzz/clusterfuzz-testcase-minimized-FuzzTIFFLoader-5363511089758208"sv),
        TEST_INPUT("tiff/fuzz/clusterfuzz-testcase-minimized-FuzzTIFFLoader-5852152004149248"sv),
        TEST_INPUT("tiff/fuzz/clusterfuzz-testcase-minimized-FuzzTIFFLoader-5903464527888384"sv),
        TEST_INPUT("tiff/fuzz/clusterfuzz-testcase-minimized-FuzzTIFFLoader-6054150979780608"sv),
        TEST_INPUT("tiff/fuzz/clusterfuzz-testcase-minimized-FuzzTIFFLoader-6276881913544704"sv),
        TEST_INPUT("tiff/fuzz/clusterfuzz-testcase-minimized-FuzzTIFFLoader-6440082630311936.fuzz"sv),
        TEST_INPUT("tiff/fuzz/clusterfuzz-testcase-minimized-FuzzTIFFLoader-6483860697186304"sv),
        TEST_INPUT("tiff/fuzz/slow-unit-02ef887f920faad55e5ec28151d00607bf2b086b.tiff"sv),
        TEST_INPUT("tiff/fuzz/slow-unit-05ee1cc597d59f1f9af6b45d470af975dd19333d.tiff"sv),
        TEST_INPUT("tiff/fuzz/slow-unit-0bdf220eda18e5358776999e94c2c9d67a55db35.tiff"sv),
        TEST_INPUT("tiff/fuzz/slow-unit-149cb59b6c0ba8ba7d2396576ed11c9f40a19713.tiff"sv),
        TEST_INPUT("tiff/fuzz/slow-unit-31a9053415ab4e40929362b13de1ba31d7cde3a9.tiff"sv),
        TEST_INPUT("tiff/fuzz/slow-unit-3d9cbd81749f40ea2f306d7bd43a26acbc29a2ba.tiff"sv),
        TEST_INPUT("tiff/fuzz/slow-unit-4277841b311ac054a274dd91760d63524714f832.tiff"sv),
        TEST_INPUT("tiff/fuzz/slow-unit-4ae44e39ec71d081fa58ab87f041729227b246d1.tiff"sv),
        TEST_INPUT("tiff/fuzz/slow-unit-5aaaae267780d7b72e92937af8fe677d85c48b69.tiff"sv),
        TEST_INPUT("tiff/fuzz/slow-unit-5c5d914b1ae9512e521cd9a8fb6b544503033346.tiff"sv),
        TEST_INPUT("tiff/fuzz/slow-unit-6eadb23505a2c1759105e22e68c4e11260b8063e.tiff"sv),
        TEST_INPUT("tiff/fuzz/slow-unit-80bb2427eef917332c2366b40fb8c2c6279313bd.tiff"sv),
        TEST_INPUT("tiff/fuzz/slow-unit-8f0546b6fe9e107df58e805f0d02b754d14a862b.tiff"sv),
        TEST_INPUT("tiff/fuzz/slow-unit-ac4bfed7fbcedd8e53b1fa7cf415121af882b015.tiff"sv),
        TEST_INPUT("tiff/fuzz/slow-unit-c2f7bdc38e7d9cbaff8c8e5f99363ee25ec6dbed.tiff"sv),
        TEST_INPUT("tiff/fuzz/slow-unit-cb47f14a856d06b5e823de8f630ec52f1e929648.tiff"sv),
        TEST_INPUT("tiff/fuzz/slow-unit-cf6894f9b445188675a8d8dd4cd85d857947a71f.tiff"sv),
        TEST_INPUT("tiff/fuzz/slow-unit-d2b5882aa20ec80cdb33dfe3e15365dbb6a8886a.tiff"sv),
        TEST_INPUT("tiff/fuzz/slow-unit-eb5ac3125b129340ce5068546282ca727936eb4f.tiff"sv),
        TEST_INPUT("tiff/fuzz/slow-unit-ece3cf3e41f99255110d605638649f99a5f2f8dd.tiff"sv),
        TEST_INPUT("tiff/fuzz/slow-unit-f37b61114c857c656ef3a2243893cfdcae10a0e9.tiff"sv),
        TEST_INPUT("tiff/fuzz/slow-unit-fc21cd65c6df8dcb3bfc63dc7a41d5f22ce60226.tiff"sv),
    };

    for (auto test_input : test_inputs) {
        auto file = TRY_OR_FAIL(Core::MappedFile::map(test_input));
        auto plugin_decoder_or_error = Gfx::TIFFImageDecoderPlugin::create(file->bytes());
        if (plugin_decoder_or_error.is_error())
            continue;
        auto frame_or_error = plugin_decoder_or_error.value()->frame(0);
        EXPECT(frame_or_error.is_error());
    }
}

TEST_CASE(test_webp_simple_lossy)
{
    auto file = TRY_OR_FAIL(Core::MappedFile::map(TEST_INPUT("webp/simple-vp8.webp"sv)));
    EXPECT(Gfx::WebPImageDecoderPlugin::sniff(file->bytes()));
    auto plugin_decoder = TRY_OR_FAIL(Gfx::WebPImageDecoderPlugin::create(file->bytes()));

    auto frame = TRY_OR_FAIL(expect_single_frame_of_size(*plugin_decoder, { 240, 240 }));

    // While VP8 YUV contents are defined bit-exact, the YUV->RGB conversion isn't.
    // So pixels changing by 1 or so below is fine if you change code.
    EXPECT_EQ(frame.image->get_pixel(120, 232), Gfx::Color(0xf2, 0xef, 0xf0, 255));
    EXPECT_EQ(frame.image->get_pixel(198, 202), Gfx::Color(0x7b, 0xaa, 0xd5, 255));
}

TEST_CASE(test_webp_simple_lossless)
{
    auto file = TRY_OR_FAIL(Core::MappedFile::map(TEST_INPUT("webp/simple-vp8l.webp"sv)));
    EXPECT(Gfx::WebPImageDecoderPlugin::sniff(file->bytes()));
    auto plugin_decoder = TRY_OR_FAIL(Gfx::WebPImageDecoderPlugin::create(file->bytes()));

    // Ironically, simple-vp8l.webp is a much more complex file than extended-lossless.webp tested below.
    // extended-lossless.webp tests the decoding basics.
    // This here tests the predictor, color, and subtract green transforms,
    // as well as meta prefix images, one-element canonical code handling,
    // and handling of canonical codes with more than 288 elements.
    // This image uses all 13 predictor modes of the predictor transform.
    auto frame = TRY_OR_FAIL(expect_single_frame_of_size(*plugin_decoder, { 386, 395 }));
    EXPECT_EQ(frame.image->get_pixel(0, 0), Gfx::Color(0, 0, 0, 0));

    // This pixel tests all predictor modes except 5, 7, 8, 9, and 13.
    EXPECT_EQ(frame.image->get_pixel(289, 332), Gfx::Color(0xf2, 0xee, 0xd3, 255));
}

TEST_CASE(test_webp_simple_lossless_alpha_used_false)
{
    // This file is identical to simple-vp8l.webp, but the `is_alpha_used` used bit is false.
    // The file still contains alpha data. This tests that the decoder replaces the stored alpha data with 0xff if `is_alpha_used` is false.
    auto file = TRY_OR_FAIL(Core::MappedFile::map(TEST_INPUT("webp/simple-vp8l-alpha-used-false.webp"sv)));
    EXPECT(Gfx::WebPImageDecoderPlugin::sniff(file->bytes()));
    auto plugin_decoder = TRY_OR_FAIL(Gfx::WebPImageDecoderPlugin::create(file->bytes()));

    auto frame = TRY_OR_FAIL(expect_single_frame_of_size(*plugin_decoder, { 386, 395 }));
    EXPECT_EQ(frame.image->get_pixel(0, 0), Gfx::Color(0, 0, 0, 0xff));
}

TEST_CASE(test_webp_extended_lossy)
{
    // This extended lossy image has an ALPH chunk for (losslessly compressed) alpha data.
    auto file = TRY_OR_FAIL(Core::MappedFile::map(TEST_INPUT("webp/extended-lossy.webp"sv)));
    EXPECT(Gfx::WebPImageDecoderPlugin::sniff(file->bytes()));
    auto plugin_decoder = TRY_OR_FAIL(Gfx::WebPImageDecoderPlugin::create(file->bytes()));

    auto frame = TRY_OR_FAIL(expect_single_frame_of_size(*plugin_decoder, { 417, 223 }));

    // While VP8 YUV contents are defined bit-exact, the YUV->RGB conversion isn't.
    // So pixels changing by 1 or so below is fine if you change code.
    EXPECT_EQ(frame.image->get_pixel(89, 72), Gfx::Color(255, 1, 0, 255));
    EXPECT_EQ(frame.image->get_pixel(174, 69), Gfx::Color(0, 255, 0, 255));
    EXPECT_EQ(frame.image->get_pixel(245, 84), Gfx::Color(0, 0, 255, 255));
    EXPECT_EQ(frame.image->get_pixel(352, 125), Gfx::Color(0, 0, 0, 128));
    EXPECT_EQ(frame.image->get_pixel(355, 106), Gfx::Color(0, 0, 0, 0));

    // Check same basic pixels as in test_webp_extended_lossless too.
    // (The top-left pixel in the lossy version is fully transparent white, compared to fully transparent black in the lossless version).
    EXPECT_EQ(frame.image->get_pixel(0, 0), Gfx::Color(255, 255, 255, 0));
    EXPECT_EQ(frame.image->get_pixel(43, 75), Gfx::Color(255, 0, 2, 255));
    EXPECT_EQ(frame.image->get_pixel(141, 75), Gfx::Color(0, 255, 3, 255));
    EXPECT_EQ(frame.image->get_pixel(235, 75), Gfx::Color(0, 0, 255, 255));
    EXPECT_EQ(frame.image->get_pixel(341, 75), Gfx::Color(0, 0, 0, 128));
}

TEST_CASE(test_webp_extended_lossy_alpha_horizontal_filter)
{
    // Also lossy rgb + lossless alpha, but with a horizontal alpha filtering method.
    // The image should look like smolkling.webp, but with a horizontal alpha gradient.
    auto file = TRY_OR_FAIL(Core::MappedFile::map(TEST_INPUT("webp/smolkling-horizontal-alpha.webp"sv)));
    EXPECT(Gfx::WebPImageDecoderPlugin::sniff(file->bytes()));
    auto plugin_decoder = TRY_OR_FAIL(Gfx::WebPImageDecoderPlugin::create(file->bytes()));

    auto frame = TRY_OR_FAIL(expect_single_frame_of_size(*plugin_decoder, { 264, 264 }));

    // While VP8 YUV contents are defined bit-exact, the YUV->RGB conversion isn't.
    // So pixels changing by 1 or so below is fine if you change code.
    // The important component in this test is alpha, and that shouldn't change even by 1 as it's losslessly compressed and doesn't use YUV.
    EXPECT_EQ(frame.image->get_pixel(131, 131), Gfx::Color(0x8f, 0x51, 0x2f, 0x4b));
}

TEST_CASE(test_webp_extended_lossy_alpha_vertical_filter)
{
    // Also lossy rgb + lossless alpha, but with a vertical alpha filtering method.
    // The image should look like smolkling.webp, but with a vertical alpha gradient, and with a fully transparent first column.
    auto file = TRY_OR_FAIL(Core::MappedFile::map(TEST_INPUT("webp/smolkling-vertical-alpha.webp"sv)));
    EXPECT(Gfx::WebPImageDecoderPlugin::sniff(file->bytes()));
    auto plugin_decoder = TRY_OR_FAIL(Gfx::WebPImageDecoderPlugin::create(file->bytes()));

    auto frame = TRY_OR_FAIL(expect_single_frame_of_size(*plugin_decoder, { 264, 264 }));

    // While VP8 YUV contents are defined bit-exact, the YUV->RGB conversion isn't.
    // So pixels changing by 1 or so below is fine if you change code.
    // The important component in this test is alpha, and that shouldn't change even by 1 as it's losslessly compressed and doesn't use YUV.
    EXPECT_EQ(frame.image->get_pixel(131, 131), Gfx::Color(0x94, 0x50, 0x32, 0x4c));
}

TEST_CASE(test_webp_extended_lossy_alpha_gradient_filter)
{
    // Also lossy rgb + lossless alpha, but with a gradient alpha filtering method.
    // The image should look like smolkling.webp, but with a few transparent pixels in the shape of a C on it. Most of the image should not be transparent.
    auto file = TRY_OR_FAIL(Core::MappedFile::map(TEST_INPUT("webp/smolkling-gradient-alpha.webp"sv)));
    EXPECT(Gfx::WebPImageDecoderPlugin::sniff(file->bytes()));
    auto plugin_decoder = TRY_OR_FAIL(Gfx::WebPImageDecoderPlugin::create(file->bytes()));

    auto frame = TRY_OR_FAIL(expect_single_frame_of_size(*plugin_decoder, { 264, 264 }));

    // While VP8 YUV contents are defined bit-exact, the YUV->RGB conversion isn't.
    // So pixels changing by 1 or so below is fine if you change code.
    // The important component in this test is alpha, and that shouldn't change even by 1 as it's losslessly compressed and doesn't use YUV.
    // In particular, the center of the image should be fully opaque, not fully transparent.
    EXPECT_EQ(frame.image->get_pixel(131, 131), Gfx::Color(0x8c, 0x47, 0x2e, 255));
}

TEST_CASE(test_webp_extended_lossy_uncompressed_alpha)
{
    auto file = TRY_OR_FAIL(Core::MappedFile::map(TEST_INPUT("webp/extended-lossy-uncompressed-alpha.webp"sv)));
    EXPECT(Gfx::WebPImageDecoderPlugin::sniff(file->bytes()));
    auto plugin_decoder = TRY_OR_FAIL(Gfx::WebPImageDecoderPlugin::create(file->bytes()));

    auto frame = TRY_OR_FAIL(expect_single_frame_of_size(*plugin_decoder, { 417, 223 }));

    // While VP8 YUV contents are defined bit-exact, the YUV->RGB conversion isn't.
    // So pixels changing by 1 or so below is fine if you change code.
    EXPECT_EQ(frame.image->get_pixel(89, 72), Gfx::Color(255, 0, 4, 255));
    EXPECT_EQ(frame.image->get_pixel(174, 69), Gfx::Color(4, 255, 0, 255));
    EXPECT_EQ(frame.image->get_pixel(245, 84), Gfx::Color(0, 0, 255, 255));
    EXPECT_EQ(frame.image->get_pixel(352, 125), Gfx::Color(0, 0, 0, 128));
    EXPECT_EQ(frame.image->get_pixel(355, 106), Gfx::Color(0, 0, 0, 0));
}

TEST_CASE(test_webp_extended_lossy_negative_quantization_offset)
{
    auto file = TRY_OR_FAIL(Core::MappedFile::map(TEST_INPUT("webp/smolkling.webp"sv)));
    EXPECT(Gfx::WebPImageDecoderPlugin::sniff(file->bytes()));
    auto plugin_decoder = TRY_OR_FAIL(Gfx::WebPImageDecoderPlugin::create(file->bytes()));

    auto frame = TRY_OR_FAIL(expect_single_frame_of_size(*plugin_decoder, { 264, 264 }));

    // While VP8 YUV contents are defined bit-exact, the YUV->RGB conversion isn't.
    // So pixels changing by 1 or so below is fine if you change code.
    EXPECT_EQ(frame.image->get_pixel(16, 16), Gfx::Color(0x3c, 0x24, 0x1a, 255));
}

TEST_CASE(test_webp_lossy_4)
{
    // This is https://commons.wikimedia.org/wiki/File:Fr%C3%BChling_bl%C3%BChender_Kirschenbaum.jpg,
    // under the Creative Commons Attribution-Share Alike 3.0 Unported license. The image was re-encoded
    // as webp at https://developers.google.com/speed/webp/gallery1 and the webp version is from there.
    // No other changes have been made.
    auto file = TRY_OR_FAIL(Core::MappedFile::map(TEST_INPUT("webp/4.webp"sv)));
    EXPECT(Gfx::WebPImageDecoderPlugin::sniff(file->bytes()));
    auto plugin_decoder = TRY_OR_FAIL(Gfx::WebPImageDecoderPlugin::create(file->bytes()));

    auto frame = TRY_OR_FAIL(expect_single_frame_of_size(*plugin_decoder, { 1024, 772 }));

    // This image tests macroblocks that have `skip_coefficients` set to true, and it test a boolean entropy decoder edge case.
    EXPECT_EQ(frame.image->get_pixel(780, 570), Gfx::Color(0x72, 0xc8, 0xf6, 255));
}

TEST_CASE(test_webp_lossy_4_with_partitions)
{
    // Same input file as in the previous test, but re-encoded to use 8 secondary partitions.
    auto file = TRY_OR_FAIL(Core::MappedFile::map(TEST_INPUT("webp/4-with-8-partitions.webp"sv)));
    EXPECT(Gfx::WebPImageDecoderPlugin::sniff(file->bytes()));
    auto plugin_decoder = TRY_OR_FAIL(Gfx::WebPImageDecoderPlugin::create(file->bytes()));

    auto frame = TRY_OR_FAIL(expect_single_frame_of_size(*plugin_decoder, { 1024, 772 }));
    EXPECT_EQ(frame.image->get_pixel(780, 570), Gfx::Color(0x73, 0xc9, 0xf9, 255));
}

TEST_CASE(test_webp_extended_lossless)
{
    auto file = TRY_OR_FAIL(Core::MappedFile::map(TEST_INPUT("webp/extended-lossless.webp"sv)));
    EXPECT(Gfx::WebPImageDecoderPlugin::sniff(file->bytes()));
    auto plugin_decoder = TRY_OR_FAIL(Gfx::WebPImageDecoderPlugin::create(file->bytes()));

    auto frame = TRY_OR_FAIL(expect_single_frame_of_size(*plugin_decoder, { 417, 223 }));

    // Check some basic pixels.
    EXPECT_EQ(frame.image->get_pixel(0, 0), Gfx::Color(0, 0, 0, 0));
    EXPECT_EQ(frame.image->get_pixel(43, 75), Gfx::Color(255, 0, 0, 255));
    EXPECT_EQ(frame.image->get_pixel(141, 75), Gfx::Color(0, 255, 0, 255));
    EXPECT_EQ(frame.image->get_pixel(235, 75), Gfx::Color(0, 0, 255, 255));
    EXPECT_EQ(frame.image->get_pixel(341, 75), Gfx::Color(0, 0, 0, 128));

    // Check pixels using the color cache.
    EXPECT_EQ(frame.image->get_pixel(94, 73), Gfx::Color(255, 0, 0, 255));
    EXPECT_EQ(frame.image->get_pixel(176, 115), Gfx::Color(0, 255, 0, 255));
    EXPECT_EQ(frame.image->get_pixel(290, 89), Gfx::Color(0, 0, 255, 255));
    EXPECT_EQ(frame.image->get_pixel(359, 73), Gfx::Color(0, 0, 0, 128));
}

TEST_CASE(test_webp_simple_lossless_color_index_transform)
{
    // In addition to testing the index transform, this file also tests handling of explicity setting max_symbol.
    auto file = TRY_OR_FAIL(Core::MappedFile::map(TEST_INPUT("webp/Qpalette.webp"sv)));
    EXPECT(Gfx::WebPImageDecoderPlugin::sniff(file->bytes()));
    auto plugin_decoder = TRY_OR_FAIL(Gfx::WebPImageDecoderPlugin::create(file->bytes()));

    auto frame = TRY_OR_FAIL(expect_single_frame_of_size(*plugin_decoder, { 256, 256 }));

    EXPECT_EQ(frame.image->get_pixel(100, 100), Gfx::Color(0x73, 0x37, 0x23, 0xff));
}

TEST_CASE(test_webp_simple_lossless_color_index_transform_pixel_bundling)
{
    struct TestCase {
        StringView file_name;
        Gfx::Color line_color;
        Gfx::Color background_color;
    };

    // The number after the dash is the number of colors in each file's color index bitmap.
    // catdog-alert-2 tests the 1-bit-per-pixel case,
    // catdog-alert-3 tests the 2-bit-per-pixel case,
    // catdog-alert-8 and catdog-alert-13 both test the 4-bits-per-pixel case.
    // catdog-alert-13-alpha-used-false is like catdog-alert-13, but with is_alpha_used set to false in the header
    // (which has the effect of ignoring the alpha information in the palette and instead always setting alpha to 0xff).
    TestCase test_cases[] = {
        { "webp/catdog-alert-2.webp"sv, Gfx::Color(0x35, 0x12, 0x0a, 0xff), Gfx::Color(0xf3, 0xe6, 0xd8, 0xff) },
        { "webp/catdog-alert-3.webp"sv, Gfx::Color(0x35, 0x12, 0x0a, 0xff), Gfx::Color(0, 0, 0, 0) },
        { "webp/catdog-alert-8.webp"sv, Gfx::Color(0, 0, 0, 255), Gfx::Color(0, 0, 0, 0) },
        { "webp/catdog-alert-13.webp"sv, Gfx::Color(0, 0, 0, 255), Gfx::Color(0, 0, 0, 0) },
        { "webp/catdog-alert-13-alpha-used-false.webp"sv, Gfx::Color(0, 0, 0, 255), Gfx::Color(0, 0, 0, 255) },
    };

    for (auto test_case : test_cases) {
        auto file = TRY_OR_FAIL(Core::MappedFile::map(MUST(String::formatted("{}{}", TEST_INPUT(""), test_case.file_name))));
        EXPECT(Gfx::WebPImageDecoderPlugin::sniff(file->bytes()));
        auto plugin_decoder = TRY_OR_FAIL(Gfx::WebPImageDecoderPlugin::create(file->bytes()));

        auto frame = TRY_OR_FAIL(expect_single_frame_of_size(*plugin_decoder, { 32, 32 }));

        EXPECT_EQ(frame.image->get_pixel(4, 0), test_case.background_color);
        EXPECT_EQ(frame.image->get_pixel(5, 0), test_case.line_color);

        EXPECT_EQ(frame.image->get_pixel(9, 5), test_case.background_color);
        EXPECT_EQ(frame.image->get_pixel(10, 5), test_case.line_color);
        EXPECT_EQ(frame.image->get_pixel(11, 5), test_case.background_color);
    }
}

TEST_CASE(test_webp_simple_lossless_color_index_transform_pixel_bundling_odd_width)
{
    StringView file_names[] = {
        "webp/width11-height11-colors2.webp"sv,
        "webp/width11-height11-colors3.webp"sv,
        "webp/width11-height11-colors15.webp"sv,
    };

    for (auto file_name : file_names) {
        auto file = TRY_OR_FAIL(Core::MappedFile::map(MUST(String::formatted("{}{}", TEST_INPUT(""), file_name))));
        auto plugin_decoder = TRY_OR_FAIL(Gfx::WebPImageDecoderPlugin::create(file->bytes()));
        TRY_OR_FAIL(expect_single_frame_of_size(*plugin_decoder, { 11, 11 }));
    }
}

TEST_CASE(test_webp_extended_lossless_animated)
{
    auto file = TRY_OR_FAIL(Core::MappedFile::map(TEST_INPUT("webp/extended-lossless-animated.webp"sv)));
    EXPECT(Gfx::WebPImageDecoderPlugin::sniff(file->bytes()));
    auto plugin_decoder = TRY_OR_FAIL(Gfx::WebPImageDecoderPlugin::create(file->bytes()));

    EXPECT_EQ(plugin_decoder->loop_count(), 42u);
    EXPECT_EQ(plugin_decoder->frame_count(), 8u);
    EXPECT(plugin_decoder->is_animated());

    EXPECT_EQ(plugin_decoder->size(), Gfx::IntSize(990, 1050));

    for (size_t frame_index = 0; frame_index < plugin_decoder->frame_count(); ++frame_index) {
        auto frame = TRY_OR_FAIL(plugin_decoder->frame(frame_index));
        EXPECT_EQ(frame.image->size(), Gfx::IntSize(990, 1050));

        // This pixel happens to be the same color in all frames.
        EXPECT_EQ(frame.image->get_pixel(500, 700), Gfx::Color::Yellow);

        // This one isn't the same in all frames.
        EXPECT_EQ(frame.image->get_pixel(500, 0), (frame_index == 2 || frame_index == 6) ? Gfx::Color::Black : Gfx::Color(0, 0, 0, 0));
    }
}

TEST_CASE(test_tvg)
{
    auto file = TRY_OR_FAIL(Core::MappedFile::map(TEST_INPUT("tvg/yak.tvg"sv)));
    EXPECT(Gfx::TinyVGImageDecoderPlugin::sniff(file->bytes()));
    auto plugin_decoder = TRY_OR_FAIL(Gfx::TinyVGImageDecoderPlugin::create(file->bytes()));

    TRY_OR_FAIL(expect_single_frame_of_size(*plugin_decoder, { 1024, 1024 }));
}

TEST_CASE(test_everything_tvg)
{
    Array file_names {
        TEST_INPUT("tvg/everything.tvg"sv),
        TEST_INPUT("tvg/everything-32.tvg"sv)
    };

    for (auto file_name : file_names) {
        auto file = TRY_OR_FAIL(Core::MappedFile::map(file_name));
        EXPECT(Gfx::TinyVGImageDecoderPlugin::sniff(file->bytes()));
        auto plugin_decoder = TRY_OR_FAIL(Gfx::TinyVGImageDecoderPlugin::create(file->bytes()));

        TRY_OR_FAIL(expect_single_frame_of_size(*plugin_decoder, { 400, 768 }));
    }
}

TEST_CASE(test_tvg_malformed)
{
    Array test_inputs = {
        TEST_INPUT("tvg/bogus-color-table-size.tvg"sv)
    };

    for (auto test_input : test_inputs) {
        auto file = TRY_OR_FAIL(Core::MappedFile::map(test_input));
        auto plugin_decoder = TRY_OR_FAIL(Gfx::TinyVGImageDecoderPlugin::create(file->bytes()));
        auto frame_or_error = plugin_decoder->frame(0);
        EXPECT(frame_or_error.is_error());
    }
}

TEST_CASE(test_tvg_rgb565)
{
    auto file = TRY_OR_FAIL(Core::MappedFile::map(TEST_INPUT("tvg/green-rgb565.tvg"sv)));
    EXPECT(Gfx::TinyVGImageDecoderPlugin::sniff(file->bytes()));
    auto plugin_decoder = TRY_OR_FAIL(Gfx::TinyVGImageDecoderPlugin::create(file->bytes()));
    auto frame = TRY_OR_FAIL(expect_single_frame_of_size(*plugin_decoder, { 100, 100 }));

    // Should be a solid dark green:
    EXPECT_EQ(frame.image->get_pixel(50, 50), Gfx::Color(0, 130, 0));
}

TEST_CASE(test_jxl_modular_simple_tree_upsample2_10bits)
{
    auto file = TRY_OR_FAIL(Core::MappedFile::map(TEST_INPUT("jxl/modular_simple_tree_upsample2_10bits_rct.jxl"sv)));
    EXPECT(Gfx::JPEGXLImageDecoderPlugin::sniff(file->bytes()));
    auto plugin_decoder = TRY_OR_FAIL(Gfx::JPEGXLImageDecoderPlugin::create(file->bytes()));

    TRY_OR_FAIL(expect_single_frame_of_size(*plugin_decoder, { 128, 128 }));

    auto frame = TRY_OR_FAIL(plugin_decoder->frame(0));
    EXPECT_EQ(frame.image->get_pixel(42, 57), Gfx::Color::from_string("#4c0072"sv));
}

static void verify_checkerboard(Gfx::ImageDecoderPlugin& decoder)
{
    TRY_OR_FAIL(expect_single_frame_of_size(decoder, { 32, 32 }));

    auto frame = TRY_OR_FAIL(decoder.frame(0));
    for (u8 i = 0; i < 32; ++i) {
        for (u8 j = 0; j < 32; ++j) {
            auto const color = frame.image->get_pixel(i, j);
            if ((i + j) % 2 == 0)
                EXPECT_EQ(color, Gfx::Color::Black);
            else
                EXPECT_EQ(color, Gfx::Color::Yellow);
        }
    }
}

TEST_CASE(test_jxl_modular_property_8)
{
    auto file = TRY_OR_FAIL(Core::MappedFile::map(TEST_INPUT("jxl/modular_property_8.jxl"sv)));
    EXPECT(Gfx::JPEGXLImageDecoderPlugin::sniff(file->bytes()));
    auto plugin_decoder = TRY_OR_FAIL(Gfx::JPEGXLImageDecoderPlugin::create(file->bytes()));

    verify_checkerboard(*plugin_decoder);
}

TEST_CASE(test_jxl_modular_squeeze)
{
    auto file = TRY_OR_FAIL(Core::MappedFile::map(TEST_INPUT("jxl/modular_squeeze.jxl"sv)));
    EXPECT(Gfx::JPEGXLImageDecoderPlugin::sniff(file->bytes()));
    auto plugin_decoder = TRY_OR_FAIL(Gfx::JPEGXLImageDecoderPlugin::create(file->bytes()));

    verify_checkerboard(*plugin_decoder);
}

TEST_CASE(test_jxl_icc)
{
    auto file = TRY_OR_FAIL(Core::MappedFile::map(TEST_INPUT("jxl/icc.jxl"sv)));
    EXPECT(Gfx::JPEGXLImageDecoderPlugin::sniff(file->bytes()));
    auto plugin_decoder = TRY_OR_FAIL(Gfx::JPEGXLImageDecoderPlugin::create(file->bytes()));
    auto icc_data = TRY_OR_FAIL(plugin_decoder->icc_data());
    EXPECT(icc_data.has_value());
    EXPECT_EQ(icc_data->size(), 2644u);

    auto profile = TRY_OR_FAIL(Gfx::ICC::Profile::try_load_from_externally_owned_memory(*icc_data));
    EXPECT(profile->is_v4());
    EXPECT(profile->primary_platform() == Gfx::ICC::PrimaryPlatform::Apple);

    verify_checkerboard(*plugin_decoder);
}

TEST_CASE(test_jxl_boxes)
{
    auto file = TRY_OR_FAIL(Core::MappedFile::map(TEST_INPUT("jxl/modular_property_8_boxed.jxl"sv)));
    EXPECT(Gfx::JPEGXLImageDecoderPlugin::sniff(file->bytes()));
    auto plugin_decoder = TRY_OR_FAIL(Gfx::JPEGXLImageDecoderPlugin::create(file->bytes()));

    verify_checkerboard(*plugin_decoder);
}

TEST_CASE(test_jxl_palette_and_groups)
{
    auto file = TRY_OR_FAIL(Core::MappedFile::map(TEST_INPUT("jxl/palette_and_groups.jxl"sv)));
    EXPECT(Gfx::JPEGXLImageDecoderPlugin::sniff(file->bytes()));
    auto plugin_decoder = TRY_OR_FAIL(Gfx::JPEGXLImageDecoderPlugin::create(file->bytes()));

    TRY_OR_FAIL(expect_single_frame_of_size(*plugin_decoder, { 64, 138 }));
    auto frame = TRY_OR_FAIL(plugin_decoder->frame(0));
    EXPECT_EQ(frame.image->get_pixel(20, 130), Gfx::Color::from_string("#0b1112"sv));
}

TEST_CASE(test_dds)
{
    Array file_names = {
        TEST_INPUT("dds/catdog-alert-29x29.dds"sv),
        TEST_INPUT("dds/catdog-alert-32x32.dds"sv)
    };

    for (auto file_name : file_names) {
        auto file = TRY_OR_FAIL(Core::MappedFile::map(file_name));
        EXPECT(Gfx::DDSImageDecoderPlugin::sniff(file->bytes()));
        auto plugin_decoder = TRY_OR_FAIL(Gfx::DDSImageDecoderPlugin::create(file->bytes()));
        TRY_OR_FAIL(expect_single_frame(*plugin_decoder));
    }
}

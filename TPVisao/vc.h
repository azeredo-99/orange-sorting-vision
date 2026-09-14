#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define _CRT_SECURE_NO_WARNINGS

#define MAX(a,b)    ((a) > (b) ? (a) : (b))

    typedef struct {
        unsigned char* data;
        int width;
        int height;
        int channels;
        int levels;
        int bytesperline;
    } IVC;

    typedef struct {
        int x, y;
        int width;
        int height;
        int area;
        int xc, yc;
        int perimeter;
        int label;
    } OVC;

    typedef struct {
        OVC blob;
        int id;
        int active;
        int frames_missing;
        char category[10];
        int diameter_mm;
        int caliber;
    } Orange;


    IVC* vc_image_new(int width, int height, int channels, int levels);
    IVC* vc_image_free(IVC* image);
    int vc_bgr_to_rgb(IVC* src);
    int vc_rgb_to_hsv(IVC* src, IVC* dst);

    int vc_hsv_segmentation(IVC* src, IVC* dst, int hmin, int hmax, int smin, int smax, int vmin, int vmax);
    int vc_binary_erode(IVC* src, IVC* dst, int kernel_size);
    int vc_binary_dilate(IVC* src, IVC* dst, int kernel_size);
    int vc_binary_open(IVC* src, IVC* dst, IVC* tmp, int kernel_size);
    int vc_binary_close(IVC* src, IVC* dst, IVC* tmp, int kernel_size);

    OVC* vc_binary_blob_labelling(IVC* src, IVC* dst, int* nlabels);
    int vc_binary_blob_info(IVC* src, OVC* blobs, int nblobs);

    int vc_check_aabb_overlap(OVC a, OVC b);
    int vc_orange_caliber_classify(float diameter_mm);
    int vc_orange_category_classify(int area, int perimeter);
    const char* vc_orange_category_to_string(int category_idx);

#ifdef __cplusplus
}
#endif
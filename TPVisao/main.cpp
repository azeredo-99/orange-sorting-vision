#define _CRT_SECURE_NO_WARNINGS
#include <string.h>
#include <iostream>
#include <opencv2/opencv.hpp>
#include "vc.h"

#define HSV_H_MIN 8
#define HSV_H_MAX 50
#define HSV_S_MIN 33
#define HSV_S_MAX 100
#define HSV_V_MIN 18
#define HSV_V_MAX 100

#define KERNEL_SIZE 7

#define MIN_AREA 5000

#define MIN_EXTENT 0.77f

#define BORDER_MARGIN 5

#define FRAMES_TTL 25

#define PIX_TO_MM (80.0f / 365.0f)

#define MAX_ORANGES 100

static void render_orange(cv::Mat& frame, const Orange& o)
{
    cv::rectangle(frame,
        cv::Point(o.blob.x, o.blob.y),
        cv::Point(o.blob.x + o.blob.width, o.blob.y + o.blob.height),
        cv::Scalar(0, 255, 0), 2);

    std::string line1 = "ID:" + std::to_string(o.id) + "  " + o.category +
        "  A:" + std::to_string(o.blob.area) +
        "  P:" + std::to_string(o.blob.perimeter);
    cv::putText(frame, line1,
        cv::Point(o.blob.x, o.blob.y - 20),
        cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 0, 0), 2);
    cv::putText(frame, line1,
        cv::Point(o.blob.x, o.blob.y - 20),
        cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(255, 255, 255), 1);

    std::string line2 = std::to_string(o.diameter_mm) + "mm  Calibre:" + std::to_string(o.caliber);
    cv::putText(frame, line2,
        cv::Point(o.blob.x, o.blob.y - 5),
        cv::FONT_HERSHEY_SIMPLEX, 0.4, cv::Scalar(0, 0, 0), 2);
    cv::putText(frame, line2,
        cv::Point(o.blob.x, o.blob.y - 5),
        cv::FONT_HERSHEY_SIMPLEX, 0.4, cv::Scalar(0, 255, 255), 1);

    const int s = 6;
    cv::line(frame,
        cv::Point(o.blob.xc - s, o.blob.yc),
        cv::Point(o.blob.xc + s, o.blob.yc),
        cv::Scalar(0, 0, 255), 2);
    cv::line(frame,
        cv::Point(o.blob.xc, o.blob.yc - s),
        cv::Point(o.blob.xc, o.blob.yc + s),
        cv::Scalar(0, 0, 255), 2);
}

int main()
{
    cv::VideoCapture capture("data/video.avi");
    if (!capture.isOpened()) {
        std::cerr << "Erro: nao foi possivel abrir o video.\n";
        return -1;
    }

    const int width = (int)capture.get(cv::CAP_PROP_FRAME_WIDTH);
    const int height = (int)capture.get(cv::CAP_PROP_FRAME_HEIGHT);
    const int total_frames = (int)capture.get(cv::CAP_PROP_FRAME_COUNT);
    const int fps = (int)capture.get(cv::CAP_PROP_FPS);

    IVC* image_rgb = vc_image_new(width, height, 3, 255);
    IVC* image_hsv = vc_image_new(width, height, 3, 255);
    IVC* image_bin = vc_image_new(width, height, 1, 255);
    IVC* image_tmp = vc_image_new(width, height, 1, 255);

    if (!image_rgb || !image_hsv || !image_bin || !image_tmp) {
        std::cerr << "Erro: falha na alocacao de memoria.\n";
        return -1;
    }

    Orange persistent_oranges[MAX_ORANGES];
    int persistent_count = 0;
    int total_count = 0;
     
    cv::Mat frame;
    while (capture.read(frame))
    {
        int nframe = (int)capture.get(cv::CAP_PROP_POS_FRAMES);

        memcpy(image_rgb->data, frame.data, (size_t)width * height * 3);
        vc_bgr_to_rgb(image_rgb);
        vc_rgb_to_hsv(image_rgb, image_hsv);

        vc_hsv_segmentation(image_hsv, image_bin,
            HSV_H_MIN, HSV_H_MAX,
            HSV_S_MIN, HSV_S_MAX,
            HSV_V_MIN, HSV_V_MAX);

        vc_binary_close(image_bin, image_bin, image_tmp, KERNEL_SIZE);

        int nblobs = 0;
        OVC* blobs = vc_binary_blob_labelling(image_bin, image_tmp, &nblobs);
        if (blobs) {
            vc_binary_blob_info(image_tmp, blobs, nblobs);
        }

        for (int j = 0; j < persistent_count; j++) {
            persistent_oranges[j].active = 0;
        }

        for (int i = 0; i < nblobs; i++)
        {
            if (blobs[i].area < MIN_AREA) continue;

            float extent = (float)blobs[i].area /
                (float)(blobs[i].width * blobs[i].height);
            if (extent < MIN_EXTENT) continue;

            bool found = false;
            for (int j = 0; j < persistent_count; j++) {
                if (vc_check_aabb_overlap(persistent_oranges[j].blob, blobs[i])) {
                    persistent_oranges[j].blob = blobs[i];
                    persistent_oranges[j].active = 1;
                    persistent_oranges[j].frames_missing = 0;
                    found = true;
                    break;
                }
            }

            if (!found) {
                bool fully_visible =
                    (blobs[i].x > BORDER_MARGIN) &&
                    (blobs[i].y > BORDER_MARGIN) &&
                    (blobs[i].x + blobs[i].width - 1 < width - BORDER_MARGIN) &&
                    (blobs[i].y + blobs[i].height - 1 < height - BORDER_MARGIN);

                if (!fully_visible) continue;

                Orange no;
                no.blob = blobs[i];
                no.id = ++total_count;
                no.active = 1;
                no.frames_missing = 0;

                float diam_px = (float)MAX(blobs[i].width, blobs[i].height);
                no.diameter_mm = (int)(diam_px * PIX_TO_MM);
                no.caliber = vc_orange_caliber_classify((float)no.diameter_mm);

                int cat_idx = vc_orange_category_classify(blobs[i].area, blobs[i].perimeter);
                strncpy(no.category, vc_orange_category_to_string(cat_idx), sizeof(no.category) - 1);
                no.category[sizeof(no.category) - 1] = '\0';

                if (persistent_count < MAX_ORANGES) {
                    persistent_oranges[persistent_count] = no;
                    persistent_count++;
                }
            }
        }

        free(blobs);

        for (int j = 0; j < persistent_count; j++) {
            if (persistent_oranges[j].active == 0) persistent_oranges[j].frames_missing++;
        }

        int write_index = 0;
        for (int read_index = 0; read_index < persistent_count; read_index++) {
            if (persistent_oranges[read_index].frames_missing <= FRAMES_TTL) {
                persistent_oranges[write_index] = persistent_oranges[read_index];
                write_index++;
            }
        }
        persistent_count = write_index;

        int current_count = 0;
        for (int j = 0; j < persistent_count; j++) {
            const Orange& po = persistent_oranges[j];
            if (po.active) {
                bool fully_visible =
                    (po.blob.x > BORDER_MARGIN) &&
                    (po.blob.y > BORDER_MARGIN) &&
                    (po.blob.x + po.blob.width - 1 < width - BORDER_MARGIN) &&
                    (po.blob.y + po.blob.height - 1 < height - BORDER_MARGIN);

                if (fully_visible) {
                    render_orange(frame, po);
                    current_count++;
                }
            }
        }

        std::string str;

        str = "Resolucao: " + std::to_string(width) + "x" + std::to_string(height);
        cv::putText(frame, str, cv::Point(20, 25), cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 0, 0), 2);
        cv::putText(frame, str, cv::Point(20, 25), cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(255, 255, 255), 1);

        str = "Total de Frames: " + std::to_string(total_frames);
        cv::putText(frame, str, cv::Point(20, 50), cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 0, 0), 2);
        cv::putText(frame, str, cv::Point(20, 50), cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(255, 255, 255), 1);

        str = "Frame Rate: " + std::to_string(fps);
        cv::putText(frame, str, cv::Point(20, 75), cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 0, 0), 2);
        cv::putText(frame, str, cv::Point(20, 75), cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(255, 255, 255), 1);

        str = "N. da Frame: " + std::to_string(nframe);
        cv::putText(frame, str, cv::Point(20, 100), cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 0, 0), 2);
        cv::putText(frame, str, cv::Point(20, 100), cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(255, 255, 255), 1);

        str = "Total: " + std::to_string(total_count);
        cv::putText(frame, str, cv::Point(20, 140), cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 0, 0), 2);
        cv::putText(frame, str, cv::Point(20, 140), cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(255, 255, 255), 1);

        str = "Atual: " + std::to_string(current_count);
        cv::putText(frame, str, cv::Point(20, 175), cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 0, 0), 2);
        cv::putText(frame, str, cv::Point(20, 175), cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(255, 255, 255), 1);

        cv::imshow("Trabalho VC", frame);

        cv::Mat debug_bin(height, width, CV_8UC1, image_bin->data);
        cv::imshow("Mascara Binaria", debug_bin);

        int key = cv::waitKey(1);
        if (key == 'q') break;
    }

    vc_image_free(image_rgb);
    vc_image_free(image_hsv);
    vc_image_free(image_bin);
    vc_image_free(image_tmp);
    cv::destroyWindow("Trabalho VC");
    cv::destroyWindow("Mascara Binaria");

    capture.release();

    return 0;
}
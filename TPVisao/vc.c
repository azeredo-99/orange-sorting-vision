#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "vc.h"


IVC * vc_image_new(int width, int height, int channels, int levels)
{
  
    if ((width <= 0) || (height <= 0) || (channels <= 0)) return NULL;
    if ((levels <= 0) || (levels > 255)) return NULL;

    IVC* image = (IVC*)malloc(sizeof(IVC));
    if (image == NULL) return NULL;

    image->width = width;
    image->height = height;
    image->channels = channels;
    image->levels = levels;
    image->bytesperline = image->width * image->channels;

    size_t datasize = (size_t)width * (size_t)height * (size_t)channels;
    image->data = (unsigned char*)malloc(datasize);

    if (image->data == NULL)
    {
        return vc_image_free(image);
    }

    return image;
}


IVC* vc_image_free(IVC* image)
{
    if (image != NULL)
    {
        if (image->data != NULL)
        {
            free(image->data);
            image->data = NULL;
        }

        free(image);
        image = NULL;
    }

    return image;
}

int vc_bgr_to_rgb(IVC* src)
{
    unsigned char* data = (unsigned char*)src->data;
    int width = src->width;
    int height = src->height;
    int bytesperline = src->width * src->channels;
    int channels = src->channels;
    int x, y;
    long int pos;
    unsigned char tmp;

   
    if ((src->width <= 0) || (src->height <= 0) || (src->data == NULL)) return 0;
    if (channels != 3) return 0;

   
    for (y = 0; y < height; y++)
    {
        for (x = 0; x < width; x++)
        {
            pos = y * bytesperline + x * channels;

            tmp = data[pos];
            data[pos] = data[pos + 2];
            data[pos + 2] = tmp;
        }
    }

    return 1;
}

int vc_rgb_to_hsv(IVC* src, IVC* dst)
{
    unsigned char* datasrc = (unsigned char*)src->data;
    int bytesperline_src = src->width * src->channels;
    int channels_src = src->channels;
    unsigned char* datadst = (unsigned char*)dst->data;
    int bytesperline_dst = dst->width * dst->channels;
    int channels_dst = dst->channels;
    int width = src->width;
    int height = src->height;
    int x, y;
    long int pos_src, pos_dst;
    float rf, gf, bf;
    float hf, sf, vf;
    float max, min, delta;

  
    if ((src->width <= 0) || (src->height <= 0) || (src->data == NULL)) return 0;
    if ((dst->width <= 0) || (dst->height <= 0) || (dst->data == NULL)) return 0;
    if ((src->width != dst->width) || (src->height != dst->height)) return 0;
    if ((channels_src != 3) || (channels_dst != 3)) return 0;

    for (y = 0; y < height; y++)
    {
        for (x = 0; x < width; x++)
        {
            pos_src = y * bytesperline_src + x * channels_src;
            pos_dst = y * bytesperline_dst + x * channels_dst;

            rf = (float)datasrc[pos_src] / 255.0f;
            gf = (float)datasrc[pos_src + 1] / 255.0f;
            bf = (float)datasrc[pos_src + 2] / 255.0f;

            max = (rf > gf) ? (rf > bf ? rf : bf) : (gf > bf ? gf : bf);
            min = (rf < gf) ? (rf < bf ? rf : bf) : (gf < bf ? gf : bf);
            delta = max - min;

            // V
            vf = max;

            // S
            if (max == 0.0f)
                sf = 0.0f;
            else
                sf = delta / max;

            // H
            if (delta == 0.0f)
            {
                hf = 0.0f;
            }
            else if (max == rf)
            {
                hf = 60.0f * (gf - bf) / delta;
                if (hf < 0.0f) hf += 360.0f;
            }
            else if (max == gf)
            {
                hf = 120.0f + 60.0f * (bf - rf) / delta;
            }
            else // max == bf
            {
                hf = 240.0f + 60.0f * (rf - gf) / delta;
            }

            // Mapear H [0, 360) -> [0, 255], S [0, 1] -> [0, 255], V [0, 1] -> [0, 255]
            datadst[pos_dst] = (unsigned char)(hf / 360.0f * 255.0f);
            datadst[pos_dst + 1] = (unsigned char)(sf * 255.0f);
            datadst[pos_dst + 2] = (unsigned char)(vf * 255.0f);
        }
    }

    return 1;
}

// hmin,hmax = [0, 360]; smin,smax = [0, 100]; vmin,vmax = [0, 100]
int vc_hsv_segmentation(IVC* src, IVC* dst, int hmin, int hmax, int smin, int smax, int vmin, int vmax)
{
    unsigned char* datasrc = (unsigned char*)src->data;
    int bytesperline_src = src->width * src->channels;
    int channels_src = src->channels;
    unsigned char* datadst = (unsigned char*)dst->data;
    int bytesperline_dst = dst->width * dst->channels;
    int channels_dst = dst->channels;
    int width = src->width;
    int height = src->height;
    int x, y;
    long int pos_src, pos_dst;
    float hf, sf, vf;

    // Verificação de erros
    if ((src->width <= 0) || (src->height <= 0) || (src->data == NULL)) return 0;
    if ((dst->width <= 0) || (dst->height <= 0) || (dst->data == NULL)) return 0;
    if ((src->width != dst->width) || (src->height != dst->height)) return 0;
    if (channels_src != 3 || channels_dst != 1) return 0;

    for (y = 0; y < height; y++)
    {
        for (x = 0; x < width; x++)
        {
            pos_src = y * bytesperline_src + x * channels_src;
            pos_dst = y * bytesperline_dst + x * channels_dst;

            // Converter de [0, 255] de volta para as escalas reais
            hf = (float)datasrc[pos_src] / 255.0f * 360.0f;       // H -> [0, 360]
            sf = (float)datasrc[pos_src + 1] / 255.0f * 100.0f;   // S -> [0, 100]
            vf = (float)datasrc[pos_src + 2] / 255.0f * 100.0f;   // V -> [0, 100]

            // Segmentação por H, S e V
            if (hmin <= hmax)
            {
                // Intervalo normal (ex: hmin=30, hmax=70)
                if (hf >= hmin && hf <= hmax && sf >= smin && sf <= smax && vf >= vmin && vf <= vmax)
                    datadst[pos_dst] = 255;
                else
                    datadst[pos_dst] = 0;
            }
            else
            {
                // Intervalo circular no H (ex: hmin=330, hmax=30 -> vermelho, passa pelo 0)
                if ((hf >= hmin || hf <= hmax) && sf >= smin && sf <= smax && vf >= vmin && vf <= vmax)
                    datadst[pos_dst] = 255;
                else
                    datadst[pos_dst] = 0;
            }
        }
    }

    return 1;
}

// FUNÇÃO: DILATAÇÃO DE IMAGEM BINÁRIA
int vc_binary_dilate(IVC* src, IVC* dst, int kernel_size)
{
    unsigned char* datasrc;
    unsigned char* datadst;
    int width, height, bytesperline;
    int x, y, kx, ky;
    long int pos, posk;
    int offset = kernel_size / 2;
    unsigned char max_val;

    // Verificação de erros
    if ((src == NULL) || (dst == NULL)) return 0;
    if ((src->data == NULL) || (dst->data == NULL)) return 0;
    if ((src->width <= 0) || (src->height <= 0)) return 0;
    if ((src->width != dst->width) || (src->height != dst->height)) return 0;
    if ((src->channels != 1) || (dst->channels != 1)) return 0;
    if (kernel_size < 3 || (kernel_size % 2) == 0) return 0;

    datasrc = (unsigned char*)src->data;
    datadst = (unsigned char*)dst->data;
    width = src->width;
    height = src->height;
    bytesperline = src->bytesperline;

    for (y = 0; y < height; y++)
    {
        for (x = 0; x < width; x++)
        {
            max_val = 0;

            // Percorrer a vizinhança NxN
            for (ky = -offset; ky <= offset; ky++)
            {
                for (kx = -offset; kx <= offset; kx++)
                {
                    int ny = y + ky;
                    int nx = x + kx;

                    // Ignorar pixels fora da imagem (estratégia de zero padding/ignorar)
                    if ((ny >= 0) && (ny < height) && (nx >= 0) && (nx < width))
                    {
                        posk = ny * bytesperline + nx;

                        if (datasrc[posk] > max_val)
                        {
                            max_val = datasrc[posk];
                        }
                    }
                }
            }

            // O pixel central recebe o valor máximo encontrado na sua vizinhança
            pos = y * bytesperline + x;
            datadst[pos] = max_val;
        }
    }

    return 1;
}

// FUNÇÃO: EROSÃO DE IMAGEM BINÁRIA
int vc_binary_erode(IVC* src, IVC* dst, int kernel_size)
{
    unsigned char* datasrc;
    unsigned char* datadst;
    int width, height, bytesperline;
    int x, y, kx, ky;
    long int pos, posk;
    int offset = kernel_size / 2;
    unsigned char min_val;

    // Verificação de erros
    if ((src == NULL) || (dst == NULL)) return 0;
    if ((src->data == NULL) || (dst->data == NULL)) return 0;
    if ((src->width <= 0) || (src->height <= 0)) return 0;
    if ((src->width != dst->width) || (src->height != dst->height)) return 0;
    if ((src->channels != 1) || (dst->channels != 1)) return 0;
    if (kernel_size < 3 || (kernel_size % 2) == 0) return 0;

    datasrc = (unsigned char*)src->data;
    datadst = (unsigned char*)dst->data;
    width = src->width;
    height = src->height;
    bytesperline = src->bytesperline;

    for (y = 0; y < height; y++)
    {
        for (x = 0; x < width; x++)
        {
            min_val = 255;

            // Percorrer a vizinhança NxN
            // Fora da imagem conta como fundo (0), tal como na erosão binária padrão:
            // isto encolhe o objeto junto aos bordos, em vez de o ignorar.
            for (ky = -offset; ky <= offset; ky++)
            {
                int ny = y + ky;

                if (ny < 0 || ny >= height)
                {
                    min_val = 0;
                    break;
                }

                for (kx = -offset; kx <= offset; kx++)
                {
                    int nx = x + kx;

                    if (nx < 0 || nx >= width)
                    {
                        min_val = 0;
                        break;
                    }

                    posk = ny * bytesperline + nx;

                    if (datasrc[posk] < min_val)
                    {
                        min_val = datasrc[posk];
                    }
                }

                if (min_val == 0) break;
            }

            // O pixel central recebe o valor mínimo encontrado na sua vizinhança
            pos = y * bytesperline + x;
            datadst[pos] = min_val;
        }
    }

    return 1;
}

int vc_binary_open(IVC* src, IVC* dst, IVC* temp, int kernel_size)
{
    if (!vc_binary_erode(src, temp, kernel_size))
    {
        return 0;
    }

    if (!vc_binary_dilate(temp, dst, kernel_size))
    {
        return 0;
    }

    return 1;
}

int vc_binary_close(IVC* src, IVC* dst, IVC* temp, int kernel_size)
{
    if (!vc_binary_dilate(src, temp, kernel_size))
    {
        return 0;
    }

    if (!vc_binary_erode(temp, dst, kernel_size))
    {
        return 0;
    }

    return 1;
}

// Etiquetagem de blobs
// src		: Imagem binária de entrada
// dst		: Imagem grayscale (irá conter as etiquetas)
// nlabels	: Endereço de memória de uma variável, onde será armazenado o número de etiquetas encontradas.
// OVC*		: Retorna um array de estruturas de blobs (objectos), com respectivas etiquetas. É necessário libertar posteriormente esta memória.
OVC* vc_binary_blob_labelling(IVC* src, IVC* dst, int* nlabels)
{
    unsigned char* datasrc = (unsigned char*)src->data;
    unsigned char* datadst = (unsigned char*)dst->data;
    int width = src->width;
    int height = src->height;
    int bytesperline = src->bytesperline;
    int channels = src->channels;
    int x, y, a, b;
    long int i, size;
    long int posX, posA, posB, posC, posD;
    int label = 1; // Etiqueta inicial.
    int num, tmplabel;
    OVC* blobs; // Apontador para array de blobs (objectos) que será retornado desta função.

    // Verificação de erros
    if ((src->width <= 0) || (src->height <= 0) || (src->data == NULL)) return NULL;
    if ((src->width != dst->width) || (src->height != dst->height) || (src->channels != dst->channels)) return NULL;
    if (channels != 1) return NULL;


    int* labeltable = (int*)calloc((size_t)width * (size_t)height, sizeof(int));
    if (labeltable == NULL) return NULL;

    // Copia dados da imagem binária para imagem grayscale
    memcpy(datadst, datasrc, bytesperline * height);


    for (i = 0, size = bytesperline * height; i < size; i++)
    {
        if (datadst[i] != 0) datadst[i] = 255;
    }

    // Limpa os rebordos da imagem binária
    for (y = 0; y < height; y++)
    {
        datadst[y * bytesperline + 0 * channels] = 0;
        datadst[y * bytesperline + (width - 1) * channels] = 0;
    }
    for (x = 0; x < width; x++)
    {
        datadst[0 * bytesperline + x * channels] = 0;
        datadst[(height - 1) * bytesperline + x * channels] = 0;
    }

    // Efectua a etiquetagem
    for (y = 1; y < height - 1; y++)
    {
        for (x = 1; x < width - 1; x++)
        {
            // Kernel:
            // A B C
            // D X

            posA = (y - 1) * bytesperline + (x - 1) * channels; // A
            posB = (y - 1) * bytesperline + x * channels; // B
            posC = (y - 1) * bytesperline + (x + 1) * channels; // C
            posD = y * bytesperline + (x - 1) * channels; // D
            posX = y * bytesperline + x * channels; // X

            // Se o pixel foi marcado
            if (datadst[posX] != 0)
            {
                if ((datadst[posA] == 0) && (datadst[posB] == 0) && (datadst[posC] == 0) && (datadst[posD] == 0))
                {
                    datadst[posX] = label;
                    labeltable[label] = label;
                    label++;
                }
                else
                {
                    num = 255;

                    // Se A está marcado
                    if (datadst[posA] != 0) num = labeltable[datadst[posA]];
                    // Se B está marcado, e é menor que a etiqueta "num"
                    if ((datadst[posB] != 0) && (labeltable[datadst[posB]] < num)) num = labeltable[datadst[posB]];
                    // Se C está marcado, e é menor que a etiqueta "num"
                    if ((datadst[posC] != 0) && (labeltable[datadst[posC]] < num)) num = labeltable[datadst[posC]];
                    // Se D está marcado, e é menor que a etiqueta "num"
                    if ((datadst[posD] != 0) && (labeltable[datadst[posD]] < num)) num = labeltable[datadst[posD]];

                    // Atribui a etiqueta ao pixel
                    datadst[posX] = num;
                    labeltable[num] = num;

                    // Actualiza a tabela de etiquetas
                    if (datadst[posA] != 0)
                    {
                        if (labeltable[datadst[posA]] != num)
                        {
                            for (tmplabel = labeltable[datadst[posA]], a = 1; a < label; a++)
                            {
                                if (labeltable[a] == tmplabel)
                                {
                                    labeltable[a] = num;
                                }
                            }
                        }
                    }
                    if (datadst[posB] != 0)
                    {
                        if (labeltable[datadst[posB]] != num)
                        {
                            for (tmplabel = labeltable[datadst[posB]], a = 1; a < label; a++)
                            {
                                if (labeltable[a] == tmplabel)
                                {
                                    labeltable[a] = num;
                                }
                            }
                        }
                    }
                    if (datadst[posC] != 0)
                    {
                        if (labeltable[datadst[posC]] != num)
                        {
                            for (tmplabel = labeltable[datadst[posC]], a = 1; a < label; a++)
                            {
                                if (labeltable[a] == tmplabel)
                                {
                                    labeltable[a] = num;
                                }
                            }
                        }
                    }
                    if (datadst[posD] != 0)
                    {
                        if (labeltable[datadst[posD]] != num)
                        {
                            for (tmplabel = labeltable[datadst[posD]], a = 1; a < label; a++)
                            {
                                if (labeltable[a] == tmplabel)
                                {
                                    labeltable[a] = num;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // Volta a etiquetar a imagem
    for (y = 1; y < height - 1; y++)
    {
        for (x = 1; x < width - 1; x++)
        {
            posX = y * bytesperline + x * channels; // X

            if (datadst[posX] != 0)
            {
                datadst[posX] = labeltable[datadst[posX]];
            }
        }
    }

    // Contagem do número de blobs
    // Passo 1: Eliminar, da tabela, etiquetas repetidas
    for (a = 1; a < label - 1; a++)
    {
        for (b = a + 1; b < label; b++)
        {
            if (labeltable[a] == labeltable[b]) labeltable[b] = 0;
        }
    }
    // Passo 2: Conta etiquetas e organiza a tabela de etiquetas, para que não hajam valores vazios (zero) entre etiquetas
    *nlabels = 0;
    for (a = 1; a < label; a++)
    {
        if (labeltable[a] != 0)
        {
            labeltable[*nlabels] = labeltable[a]; // Organiza tabela de etiquetas
            (*nlabels)++; // Conta etiquetas
        }
    }

    // Se não há blobs
    if (*nlabels == 0)
    {
        free(labeltable);
        return NULL;
    }

    // Cria lista de blobs (objectos) e preenche a etiqueta
    blobs = (OVC*)calloc((*nlabels), sizeof(OVC));
    if (blobs != NULL)
    {
        for (a = 0; a < (*nlabels); a++) blobs[a].label = labeltable[a];
    }
    else
    {
        free(labeltable);
        return NULL;
    }

    free(labeltable);
    return blobs;
}

int vc_binary_blob_info(IVC* src, OVC* blobs, int nblobs)
{
    unsigned char* data = (unsigned char*)src->data;
    int width = src->width;
    int height = src->height;
    int bytesperline = src->bytesperline;
    int channels = src->channels;
    int x, y, i;
    long int pos;
    int xmin, ymin, xmax, ymax;
    long int sumx, sumy;

    // Verificação de erros
    if ((src->width <= 0) || (src->height <= 0) || (src->data == NULL)) return 0;
    if (channels != 1) return 0;

    // Conta área de cada blob
    for (i = 0; i < nblobs; i++)
    {
        xmin = width - 1;
        ymin = height - 1;
        xmax = 0;
        ymax = 0;

        sumx = 0;
        sumy = 0;

        blobs[i].area = 0;
        blobs[i].perimeter = 0;

        for (y = 1; y < height - 1; y++)
        {
            for (x = 1; x < width - 1; x++)
            {
                pos = y * bytesperline + x * channels;

                if (data[pos] == blobs[i].label)
                {
                   
                    blobs[i].area++;

                   
                    sumx += x;
                    sumy += y;

                    
                    if (xmin > x) xmin = x;
                    if (ymin > y) ymin = y;
                    if (xmax < x) xmax = x;
                    if (ymax < y) ymax = y;

                 
                    if ((data[pos - 1] != blobs[i].label) || (data[pos + 1] != blobs[i].label) || (data[pos - bytesperline] != blobs[i].label) || (data[pos + bytesperline] != blobs[i].label))
                    {
                        blobs[i].perimeter++;
                    }
                }
            }
        }

      
        blobs[i].x = xmin;
        blobs[i].y = ymin;
        blobs[i].width = (xmax - xmin) + 1;
        blobs[i].height = (ymax - ymin) + 1;

        
        blobs[i].xc = (int)(sumx / MAX(blobs[i].area, 1));
        blobs[i].yc = (int)(sumy / MAX(blobs[i].area, 1));
    }

    return 1;
}

int vc_check_aabb_overlap(OVC a, OVC b)
{
    return (a.x < (b.x + b.width) && (a.x + a.width) > b.x &&
        a.y < (b.y + b.height) && (a.y + a.height) > b.y);
}

typedef struct {
    int caliber;
    float min_mm;
    float max_mm;
} VC_CALIBER_RANGE;

static const VC_CALIBER_RANGE CALIBER_RANGES[] = {
    { 1, 87.0f, 100.0f },
    { 2, 84.0f, 96.0f },
    { 3, 81.0f, 92.0f },
    { 4, 77.0f, 88.0f },
    { 5, 73.0f, 84.0f },
    { 6, 70.0f, 80.0f },
    { 7, 67.0f, 76.0f },
    { 8, 64.0f, 73.0f },
    { 9, 62.0f, 70.0f },
    { 10, 60.0f, 68.0f },
    { 11, 58.0f, 66.0f },
    { 12, 56.0f, 63.0f },
    { 13, 53.0f, 60.0f }
};

int vc_orange_caliber_classify(float diameter_mm) {
    if (diameter_mm < 53.0f) return -1;  
    if (diameter_mm >= 100.0f) return 0;  

    int best_caliber = -1;
    float best_distance = 1e30f;
    int i;

    for (i = 0; i < (int)(sizeof(CALIBER_RANGES) / sizeof(CALIBER_RANGES[0])); i++)
    {
        float center, distance;

        if (diameter_mm < CALIBER_RANGES[i].min_mm || diameter_mm > CALIBER_RANGES[i].max_mm) continue;

        center = (CALIBER_RANGES[i].min_mm + CALIBER_RANGES[i].max_mm) / 2.0f;
        distance = fabsf(diameter_mm - center);

        if (distance < best_distance)
        {
            best_distance = distance;
            best_caliber = CALIBER_RANGES[i].caliber;
        }
    }

    return best_caliber;
}

int vc_orange_category_classify(int area, int perimeter) {
    if (perimeter == 0)
    {
        return -1;
    }

   
    float circularity = (4.0f * 3.14159f * (float)area) / ((float)perimeter * (float)perimeter);


    float deformation = fabsf(1.0f - circularity) * 100.0f;
    if (deformation <= 2.0f)
    {
        return 0; // Extra
    }
    if (deformation <= 5.0f)
    {
        return 1; // Class I
    }

    return 2; // Class II
}

const char* vc_orange_category_to_string(int category_idx) {
    switch (category_idx) {
    case 0: return "Extra";
    case 1: return "Class I";
    case 2: return "Class II";
    case -1: return "Erro";
    default: return "Unknown";
    }
}
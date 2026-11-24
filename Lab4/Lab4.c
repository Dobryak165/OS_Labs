/*
 * Лабораторная работа №4. Динамические библиотеки. Вариант 1.
 *
 * Сборка в Linux из каталога с исходниками:
 * gcc -std=c11 -O2 -Wall -Wextra -Werror -fPIC -shared Lab4.c -o librect.so -lm
 * gcc -std=c11 -O2 -Wall -Wextra -Werror -DLAB_TRAP -fPIC -shared Lab4.c -o libtrap.so -lm
 * gcc -std=c11 -O2 -Wall -Wextra -Werror Main1.c -o program1 -L. -lrect -Wl,-rpath,'$ORIGIN'
 * gcc -std=c11 -O2 -Wall -Wextra -Werror Main2.c -o program2 -ldl
 *
 * Запуск из этого же каталога: ./program1 или ./program2.
 * 1 a b e — интеграл, 2 a dx — производная, 0 — переключение в program2.
 * Углы в радианах, дроби вводятся с точкой. Ctrl+D завершает ввод.
 */

#include <float.h>
#include <math.h>
#include "Lab4.h"

float sin_integral(float a, float b, float e) {
    if (!isfinite(a) || !isfinite(b) || !isfinite(e) || e <= 0.0f) {
        return NAN;
    }

    double start = fmin(a, b);
    double end = fmax(a, b);
    double bound = fmax(fabs(start), fabs(end));
    if (start < end && bound + e == bound) {
        return NAN;
    }

    double result = 0.0;
#ifdef LAB_TRAP
    double sin_left = sin(start);
#endif

    double left = start;
    while (left < end) {
        double right = fmin(left + e, end);
        if (right <= left) {
            return NAN;
        }
#ifndef LAB_TRAP
        result += sin(left) * (right - left);
#else
        double sin_right = sin(right);
        result += (sin_left + sin_right) * (right - left) / 2.0;
        sin_left = sin_right;
#endif
        left = right;
    }

    if (a > b) result = -result;
    if (fabs(result) > FLT_MAX) return NAN;
    return (float)result;
}

float cos_derivative(float a, float dx) {
    if (!isfinite(a) || !isfinite(dx) || dx == 0.0f) {
        return NAN;
    }

    double x = a;
    double h = dx;
#ifndef LAB_TRAP
    return (float)((cos(x + h) - cos(x)) / h);
#else
    return (float)((cos(x + h) - cos(x - h)) / (2.0 * h));
#endif
}

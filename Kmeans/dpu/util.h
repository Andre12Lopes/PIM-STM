#ifndef _UTIL_H_
#define _UTIL_H_

static inline intptr_t
double2intp(float val)
{
    union {
        intptr_t i;
        float d;
    } convert;

    convert.d = val;

    return convert.i;
}

static inline intptr_t *
doublep2intpp(double *val)
{
    union {
        intptr_t *i;
        double *d;
    } convert;
    convert.d = val;
    return convert.i;
}

static inline float
intp2double(intptr_t val)
{
    union {
        intptr_t i;
        float d;
    } convert;

    convert.i = val;

    return convert.d;
}

#endif /* _UTIL_H_ */

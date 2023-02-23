#define POW_REC_THRESHOLD 8

static int pow_rec(int base, int exp) {
    if (exp > 1) {
        int partial = pow_rec(base, exp >> 1);
        int remainder = (exp & 1) ? base : 1;
        return partial * partial * remainder;
    }
    if (exp == 1)
        return base;
    else
        return 1;
}

int _pow(int exp, int base) {
    if (exp < POW_REC_THRESHOLD) {
        int result;
        for (result = 1; exp --; result *= base)
            ;
        return result;
    }
    return pow_rec(base, exp);
}

int _strcmp(const char *s2, const char *s1)
{
    for (; *s1 && *s1 == *s2; s1++, s2++)
        ;
	return *(const unsigned char*) s1 - *(const unsigned char*) s2;
}

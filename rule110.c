int printn_int(int _1);
int print_char(int _1);
int print_newline();

void print_cell(int val) {
    if (val == 1) {
        print_char(35);
    } else {
        print_char(46);
    }
}

int calc_rule(int a, int b, int c) {
    if (a == 0 && b == 0 && c == 0) return 0;
    else if (a == 0 && b == 0 && c == 1) return 1;
    else if (a == 0 && b == 1 && c == 0) return 1;
    else if (a == 0 && b == 1 && c == 1) return 1;
    else if (a == 1 && b == 0 && c == 0) return 0;
    else if (a == 1 && b == 0 && c == 1) return 1;
    else if (a == 1 && b == 1 && c == 0) return 1;
    else if (a == 1 && b == 1 && c == 1) return 0;
    else                                 return 2;
}

int main() {
    int j = 0;
    int i = 0;
    int b[22];
    int a[22];

    a[0] = 0;
    a[1] = 0;
    a[2] = 0;
    a[3] = 0;
    a[4] = 0;
    a[5] = 0;
    a[6] = 0;
    a[7] = 0;
    a[8] = 0;
    a[9] = 0;
    a[10] = 0;
    a[11] = 0;
    a[12] = 0;
    a[13] = 0;
    a[14] = 0;
    a[15] = 0;
    a[16] = 0;
    a[17] = 0;
    a[18] = 0;
    a[19] = 0;
    a[20] = 0;
    a[21] = 1;

    START:
    print_cell(a[i + 1]);
    b[i + 1] = calc_rule(a[i], a[i + 1], a[i + 2]);

    if (i == 20) {
        i = 0;
        print_newline();
        j = j + 1;
        a[1] = b[1];
        a[2] = b[2];
        a[3] = b[3];
        a[4] = b[4];
        a[5] = b[5];
        a[6] = b[6];
        a[7] = b[7];
        a[8] = b[8];
        a[9] = b[9];
        a[10] = b[10];
        a[11] = b[11];
        a[12] = b[12];
        a[13] = b[13];
        a[14] = b[14];
        a[15] = b[15];
        a[16] = b[16];
        a[17] = b[17];
        a[18] = b[18];
        a[19] = b[19];
        a[20] = b[20];
    } else {
        i = i + 1;
    }

    if (j < 40) {
        goto START;
    }

    return 0;
}
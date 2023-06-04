int add_x_ignore_y_add_1(int x, int y) {
    int two;
    two = 2;

    y = 0;

    return two + x + y;
}

int main() {
    int tewnty;
    tewnty = 20;
    
    int ten;
    ten = 10;

    int twelve;
    twelve = 12;

    return tewnty * add_x_ignore_y_add_1(3, 1337) - (twelve + twelve + ten) - 2;
}
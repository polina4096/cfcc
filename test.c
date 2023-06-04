int return_two() {
    int two = 2;
    return two;
}

int add_x_ignore_y_add_1(int x, int y) {
    int two = return_two();

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

int test2() {
    int _1;
    int _2;
    int _3;
    int _4;
    int _5;
    int _6;
}

int test() {
    int _1;
    int _2;
    int _3;

    {
        int _4;
        int _1;
        int _2;
        int _3;

        {
            int _5;
        }
    }

    int _6;
}
int add_x_ignore_y_add_1(int x, int y) {
    int two = 2;

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

int test() {
    {
        int a;
        a = 4;
        return a;

        {
            int a;
            a = 6;
            return a + 4;
        }
    }

    int a;
    a = 2;
    return a;
}
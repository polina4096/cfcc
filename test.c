int main() {
    int a = 2;

    if (a == 1) {
        return 11;
    } else if (a == 2) {
        return 22;
    } else {
        a = 8;
    }

    if (a == 1) {
        return 11;
    } else if (a == 2) {
        return 22;
    } else {
        a = 8;
    }

    return a;
}

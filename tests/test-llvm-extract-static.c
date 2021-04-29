#include<stdio.h>

static int data = 99;

int a() {
	return 9;
}

int b() {
	return a() + data + 5;
}

int c() {
	data += 1;
	return a() + data + 1;
}

int main() {
	printf("%d %d\n", b(), c());
}



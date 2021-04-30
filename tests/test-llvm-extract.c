#include<stdio.h>

int a() {
	return 9;
}

int b() {
	return a() + 5;
}

int c() {
	return a() + 1;
}

int main() {
	printf("%d %d\n", b(), c());
}



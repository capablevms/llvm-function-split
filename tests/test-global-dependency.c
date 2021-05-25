// Test if the "a" and "c" string constants will be moved
// along with the `data` GlobalVariable.
// It also tests if the referances to the a and cc functions
// will be preserved properly when moving.

#include<stdio.h>

struct Pair {
	char* name;
	int (*function)();
};

int a() {
	return 9;
}

int b() {
	return a() + 5;
}

int cc() {
	return a() + 1;
}

static struct Pair data[] = {
	{ "a", a },
	{ "c", cc },
	0
};

int len(struct Pair list[]) {
	int i = 0;
	while(list[i].name != NULL) {
		i++;	
	}
	return i;
}

int main() {
	int lenght_data = len(data);
	for(int i = 0; i < lenght_data; i++) {
		printf("name: %s, function: %p, exec: %d \n", data[i].name, data[i].function, data[i].function());
	}
}



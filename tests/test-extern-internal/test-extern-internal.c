__attribute__((visibility("internal"))) extern const unsigned char keep_me_hidden[5];
const unsigned char keep_me_hidden[5] = {0x00, 0x00, 0x00, 0x00, 0x00};

int main(void)
{
	// Just read the variable
	keep_me_hidden[3];
	return 0;
}
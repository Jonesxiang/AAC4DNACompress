#include<stdio.h>
#include<stdlib.h>
#include<memory.h>
#include<malloc.h>
#include"aac.h"

#define AAC_CODE_VALUE_BITS 16

#define AAC_TOP_VALUE       (((unsigned long)1<<AAC_CODE_VALUE_BITS)-1)

#define AAC_FIRST_QTR       (AAC_TOP_VALUE/4+1)
#define AAC_HALF            (2*AAC_FIRST_QTR)
#define AAC_THIRD           (3*AAC_FIRST_QTR)

#define AAC_NO_OF_CHARS     4
#define AAC_EOF_SYMBOL      AAC_NO_OF_CHARS
#define AAC_NO_OF_SYMBOLS   (AAC_NO_OF_CHARS+1)

#define AAC_MAX_FREQUENCY   (AAC_TOP_VALUE>>2)
#define AAC_DOWN_SCALE      2

void (*aac_output_char)(char);

typedef struct
{
	int id;
	int freq_len;
	int * freq;
	int cum_freq_len;
	int * cum_freq;
} AACElement;

int aac_char_to_symbol(char c)
{
	switch (c) {
		case 'A':
			return 0;
		case 'C':
			return 1;
		case 'G':
			return 2;
		case 'T':
			return 3;
	}
	fprintf(stderr, "Bad char:%c for DNA compress - acc_char_to_symbol()!\n", c);
	exit(1);
}

char aac_symbol_to_char[] = {'A', 'C', 'G', 'T'};

void aac_init_AACElement(AACElement * element, int id, int no_of_symbols)
{
	element->id = id;
	element->freq_len = no_of_symbols;
	element->freq = (int*) malloc(sizeof(int) * no_of_symbols);
	element->cum_freq_len = no_of_symbols + 1;
	element->cum_freq = (int*) malloc(sizeof(int) * (no_of_symbols + 1));
}

void aac_release_AACElement(AACElement * element) 
{
	free(element->freq);
	free(element->cum_freq);
}

void aac_release_AACElements(AACElement * elements, int elements_len)
{	
	int i;
	for (i = 0; i < elements_len; i++) {
		aac_release_AACElement(elements + i);
	}
	free(elements);
}

void aac_update_cum_freq(int * cum_freq, int cum_freq_len, int * freq, int freq_len)
{
	cum_freq[cum_freq_len - 1] = 0;

	int i;
	for (i = cum_freq_len - 1; i > 0; i--) {
		cum_freq[i-1] = cum_freq[i] + freq[i-1];
	}
}

void aac_init_freq(int * freq, int freq_len)
{
	int i;
	for (i = 0; i < freq_len; i++) {
		freq[i] = 1;
	}
}

AACElement * aac_get_AACElements(int order, int * elements_len)
{
	int i;
	int len = AAC_NO_OF_CHARS;
	for (i = 0; i < order; i++) {
		len += len * AAC_NO_OF_CHARS;
	}
	*elements_len = len;
	AACElement * elements = (AACElement*) malloc(sizeof(AACElement) * len);
	return elements;
}

void aac_start_model(AACElement * elements, int elements_len)
{
	int i;
	for (i = 0; i < elements_len; i++) {
		aac_init_AACElement(elements+i, i, AAC_NO_OF_SYMBOLS);
		aac_init_freq(elements[i].freq, elements[i].freq_len);
		aac_update_cum_freq(elements[i].cum_freq, elements[i].cum_freq_len, 
				elements[i].freq, elements[i].freq_len);
	}
}

void aac_update_model(AACElement * element, int symbol)
{
	if (symbol >= AAC_NO_OF_SYMBOLS) {
		fprintf(stderr, "Bad symbol:%d for aac_update_cum_freq()\n", symbol);
		exit(1);
	}
	element->freq[symbol]++;
	if (element->cum_freq[0] >= AAC_MAX_FREQUENCY) {
		int i;
		int len = element->freq_len;
		for (i = 0; i < len; i++)
			element->freq[i] = element->freq[i]/AAC_DOWN_SCALE + 1;
	}
	aac_update_cum_freq(element->cum_freq, element->cum_freq_len, 
				element->freq, element->freq_len);
}



int aac_update_AACElement_id(int * idc, int idc_len, int symbol)
{
	int i;
	for (i = idc_len - 1; i > 0; i--) {
		idc[i] = idc[i-1];
	}
	idc[0] = symbol;

	int id = 0;
	for (i = idc_len - 1; i >= 0; i--) {
		id = id * AAC_NO_OF_CHARS + idc[i];
	}
	return id;
}

/******************** encode **************************/

int aac_buffer;
int aac_bits_to_go;
int aac_bits_to_follow;
unsigned long aac_low;
unsigned long aac_high;

unsigned long aac_value;
int aac_garbage_bits;
char * aac_code;
int aac_decode_index;
int aac_code_len;

void aac_start_outputing_bits()
{
	aac_buffer = 0;
	aac_bits_to_go = 8;                          
}


void aac_output_bit(int bit)
{
	aac_buffer >>= 1;
	if (bit) aac_buffer |= 0x80;
	aac_bits_to_go -= 1;
	if (aac_bits_to_go==0) {
		aac_output_char((char) aac_buffer);
		aac_bits_to_go = 8;
	}
}

void aac_done_outputing_bits()
{
	aac_buffer >>= aac_bits_to_go;
	aac_output_char((char) aac_buffer);
}

void aac_start_encoding()
{
	aac_low = 0;
	aac_high = AAC_TOP_VALUE;
	aac_bits_to_follow = 0;
}

void aac_bit_plus_follow(int bit)
{
	aac_output_bit(bit);
	while (aac_bits_to_follow > 0) {
		aac_output_bit(!bit);
		aac_bits_to_follow -= 1;
	}
}

void aac_done_encoding()
{
	aac_bits_to_follow += 1;
	if (aac_low < AAC_FIRST_QTR) aac_bit_plus_follow(0);
	else aac_bit_plus_follow(1);
}

void aac_encode_symbol(int * cum_freq, int symbol)
{
	unsigned long range = (unsigned long)(aac_high - aac_low) + 1;

	aac_high = aac_low + (range*cum_freq[symbol])/cum_freq[0]-1;
	aac_low = aac_low + (range*cum_freq[symbol+1])/cum_freq[0];

	for (;;) {
		if (aac_high < AAC_HALF) {
			aac_bit_plus_follow(0);
		} else if (aac_low >= AAC_HALF) {
			aac_bit_plus_follow(1);
			aac_low -= AAC_HALF;
			aac_high -= AAC_HALF;
		} else if (aac_low >= AAC_FIRST_QTR && aac_high < AAC_THIRD) {
			aac_bits_to_follow += 1;
			aac_low -= AAC_FIRST_QTR;
			aac_high -= AAC_FIRST_QTR;
		} else break;
		aac_low = 2*aac_low;
		aac_high = 2*aac_high+1;
	}
}

void adaptive_arithmetic_encode(char * chs, int chs_len, void (*output_char)(char), int order)
{
	aac_output_char = output_char;
	int elements_len;
	AACElement * elements = aac_get_AACElements(order, &elements_len);
	int * idc = (int*) malloc(sizeof(int) * order);
	memset(idc, 0, sizeof(int) * order);

	aac_start_model(elements, elements_len);
	aac_start_outputing_bits();
	aac_start_encoding();
	int i;
	int id = 0;
	for (i = 0; i < chs_len; i++) {
		int symbol = aac_char_to_symbol(chs[i]);
		aac_encode_symbol(elements[id].cum_freq, symbol);
		aac_update_model(elements + id, symbol);
		id = aac_update_AACElement_id(idc, order, symbol);
	}
	aac_encode_symbol(elements[id].cum_freq, AAC_EOF_SYMBOL);
	aac_done_encoding();
	aac_done_outputing_bits();
}


/******************** decode **************************/
void aac_start_inputing_bits()
{
	aac_bits_to_go = 0;
	aac_garbage_bits = 0;
}

int aac_input_bit()
{
	int t;
	if (aac_bits_to_go == 0) {
		aac_buffer = aac_code[aac_decode_index];
		aac_decode_index++;
		if(aac_decode_index > aac_code_len ) {
			aac_garbage_bits += 1;
			if (aac_garbage_bits > AAC_CODE_VALUE_BITS - 2) {
				fprintf(stderr, "Bad input file\n");
				exit(1);
			}
		}
		aac_bits_to_go = 8;
	}
	t = aac_buffer&1;
	aac_buffer >>= 1;
	aac_bits_to_go -= 1;
	return t;
}

void aac_start_decoding()
{
	aac_decode_index = 0;
	int i;
	aac_value = 0;
	for (i = 1; i<= AAC_CODE_VALUE_BITS; i++) {
		aac_value = 2*aac_value+aac_input_bit();
	}
	aac_low = 0;
	aac_high = AAC_TOP_VALUE;
}


int aac_decode_symbol(int * cum_freq)
{
	unsigned long range = (unsigned long)(aac_high - aac_low) + 1;
	int cum = (((unsigned long)(aac_value - aac_low) + 1) * cum_freq[0] - 1)/range;

	int symbol;
	for (symbol = 0; cum_freq[symbol+1]>cum; symbol++);
	aac_high = aac_low + (range*cum_freq[symbol])/cum_freq[0]-1;
	aac_low = aac_low + (range*cum_freq[symbol+1])/cum_freq[0];
	for (;;) {
		if (aac_high < AAC_HALF) {
		
		} else if (aac_low >= AAC_HALF) {
			aac_value -= AAC_HALF;
			aac_low -= AAC_HALF;
			aac_high -= AAC_HALF; 
		} else if (aac_low >= AAC_FIRST_QTR && aac_high <AAC_THIRD) {
			aac_value -= AAC_FIRST_QTR;
			aac_low -= AAC_FIRST_QTR;
			aac_high -= AAC_FIRST_QTR;
		} else break;
		aac_low = 2*aac_low;
		aac_high = 2*aac_high+1;
		aac_value = 2*aac_value+aac_input_bit();
	}
	return symbol;
}

void adaptive_arithmetic_decode(char * chs, int chs_len, void (*output_char)(char), int order)
{
	aac_code = chs;
	aac_code_len = chs_len;

	int elements_len;
	AACElement * elements = aac_get_AACElements(order, &elements_len);
	int * idc = (int*) malloc(sizeof(int) * order);
	memset(idc, 0, sizeof(int) * order);

	aac_start_model(elements, elements_len);

	aac_start_inputing_bits();
	aac_start_decoding();
	int id = 0;
	for (;;) {
		int symbol = aac_decode_symbol(elements[id].cum_freq);
		if (symbol==AAC_EOF_SYMBOL) break;
		char ch = aac_symbol_to_char[symbol];
		output_char(ch);
		aac_update_model(elements + id, symbol);
		id = aac_update_AACElement_id(idc, order, symbol);
	}
}

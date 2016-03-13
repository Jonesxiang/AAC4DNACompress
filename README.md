#AAC4DNACompress
    对于只包含"ACGT"的DNA序列的自适应算术编码
    使用 gcc -o aac main.c aac.c file.c编译源文件
    使用 aac -e dna.txt -o 3 编码DNA文件，-o指定上下文阶数为3
    对应 aac -d dna.txt.ac -o 3 解码

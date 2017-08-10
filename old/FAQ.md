

## FAQ about HapCUT input and output files 

1. hapcut does not handle padded alignments in BAM (CGI)

2. hapcut requires variants to be called using samtools or similar program and VCF file to provided as input 

3. hapcut requires mate chromosome to be set in BAM file, this is important for Hi-C or RNA-seq data where reads may be mapped separately
for each paired-end read

4. hapcut is designed for diploid organisms but may work for triploid (tetraploid....)  wheat Inference of haplotypic phase and missing genotypes in polyploid organisms and variable copy number genomic regions

5. hapcut can perform indel realignment when extracting haplotype-informative reads from BAM files (use --indels 1 option for this and
provide a reference fasta file) 

6. hapcut requires the fasta file to be indexed using samtools or a similar program 



# HapCUT input file format:


In the hapcut input file, each line has information from a single fragment about the alleles (0/1) that it covers.

Column 1 is the number of blocks (consecutive set of SNPs covered by the fragment).
Column 2 is the fragment id.
Column 3 is the offset of the first block of SNPs covered by the fragment followed by the alleles at the SNPs in this block.
Column 5 is the offset of the second block of SNPs covered by the fragment followed by the alleles at the SNPs in this block.
...

The last column is a string with the quality values (Sanger fastq format) for all the alleles covered by the fragment (concatenated for all blocks).

For example, if a read/fragment covers SNPs 2,3 and 5 with the alleles 0, 1 and 0 respectively, then the input will be:

2 read_id 2 01 5 0 AAC

Here AAC is the string corresponding to the quality values at the three alleles. The encoding of 0/1 is arbitrary but following the VCF format, 0 is reference and 1 is alternate.

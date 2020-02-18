#include<stdio.h>
#include<stdlib.h>
#include<time.h>
#include<math.h>
#include<string.h>

// data structures and global variables 
#include "common.h" // common functions
#include "fragments.h" // fragment likelihood, haplotype assignment
#include "variantgraph.h" // 
#include "hapcontig.h" // haplotype contigs or blocks 

// input related
#include "optionparser.c" // global variables and parse command line arguments to change these variables 
#include "readinputfiles.h" // read fragment matrix generated by extracthairs
#include "readvcf.h" // read VCF file 
//#include "data_stats.c"

// maxcut optimization related
#include "pointerheap.h"  // heap for max-cut greedy algorithm
#include "find_maxcut.c"   // function compute_good_cut
#include "post_processing.c"  // post-process haplotypes to remove SNVs with low-confidence phasing, split blocks 
#include "hic.h"  // HiC relevant functions

// output related
#include "output_phasedvcf.c" // output VCF phased file

// IMPORTANT NOTE: all SNPs start from 1 instead of 0 and all offsets are 1+ in fragment file

// captures all the relevant data structures for phasing as a single structure
typedef struct    
{
	struct fragment* Flist; int fragments;
	struct fragment* full_Flist; int full_fragments; // full fragment list
	struct SNPfrags* snpfrag; int snps;
	char* HAP1; // single haplotype for het variants, remaining variants are set to '-' (missing)
	char** fullhaps; // haplotypes for all variants and ploidy
	struct BLOCK* clist; int components;
	int filtered; // whether fragment list used for phasing is filtered or identical to full_Flist
	char graph_ready; // has read-variant graph been constructed or not
	PVAR* varlist;
	int MINQ; // minimum quality value filter
} DATA;

#include "aux_hapcut2.c" // some functions moved here
#include "phased_genotyping.c" // update genotype of each variant, unphase some variants

void init_random_hap(struct SNPfrags* snpfrag,int snps,char* HAP1)
{
    int i=0;
    for (i = 0; i < snps; i++) 
    {
	// this should be checked only after fragments per SNV have been counted
        if (snpfrag[i].phase == '0')  HAP1[i] = '-';  
        else if (drand48() < 0.5) HAP1[i] = '0';
        else HAP1[i] = '1';
    }
}

int build_readvariant_graph(DATA* data,int long_reads) 
{
    int i=0; int components=0;
    update_snpfrags(data->Flist, data->fragments, data->snpfrag, data->snps);
    // 10/25/2014, edges are only added between adjacent nodes in each fragment and used for determining connected components...
   // for (i = 0; i < snps; i++) snpfrag[i].elist = (struct edge*) malloc(sizeof (struct edge)*(snpfrag[i].edges+1)); // # of edges calculated in update_snpfrags function  
    //for(i=0;i<data->snps;i++) fprintf(stdout,"%d %d \n",i,data->snpfrag[i].edges);
    if (long_reads  ==0)  add_edges(data->Flist,data->fragments,data->snpfrag,data->snps,&components); // add all edges
    else add_edges_longreads(data->Flist,data->fragments,data->snpfrag,data->snps,&components);
    // length of telist is smaller since it does not contain duplicates, calculated in add_edges 
    for (i = 0; i < data->snps; i++) data->snpfrag[i].telist = (struct edge*) malloc(sizeof (struct edge)*(data->snpfrag[i].edges+1));
    data->components = components;
    data->graph_ready = '1';
}

int FILTER_HETS=1;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int diploid_haplotyping(DATA* data) {
    int i=0;

    // INITIALIZE VARIANTS TO BE PHASED based on current genotype
    for (i=0;i<data->snps;i++)
    {
	data->snpfrag[i].phase = '1'; 
	// ignore homzygous variants, for such variants, the HAP1[i] value is set to '-' to avoid using them for phasing  
	if (data->snpfrag[i].genotypes[0] == data->snpfrag[i].genotypes[2]) data->snpfrag[i].phase = '0'; 
	//if (data->graph_ready == '1' && data->snpfrag[i].frags ==0) data->snpfrag[i].phase = '0'; // this causes bug since graph
	if (SNVS_BEFORE_INDELS && data->snpfrag[i].is_indel == '1') data->snpfrag[i].phase = '0';
    }

    // Generate reduced fragment list, 'snpfrag[i].phase' needs to be set, also removes singleton fragments
    if (FILTER_HETS ==1) {
    	    data->Flist = calloc(sizeof(struct fragment),data->full_fragments);
	    data->fragments = filter_fragments(data->full_Flist,data->full_fragments,data->snpfrag,data->Flist);
	    fprintf(stderr,"constructing reduced fragment list for phasing heterozygous variant, initial fragments: %d removed %d\n",data->full_fragments,data->full_fragments-data->fragments);
            qsort(data->Flist, data->fragments, sizeof (struct fragment), fragment_compare);
    }
    else
    {
          data->fragments = data->full_fragments; data->Flist = data->full_Flist; 
    }

    // BUILD FRAGMENT-VARIANT GRAPH for to-be-phased variants
    fprintf_time(stderr, "building read-variant graph for phasing\n");
    build_readvariant_graph(data,LONG_READS); 
    
    // INITIALIZE HAPLOTYPES, this should be changed to only updating 'phased' set and set HAP1[x] = '-' 
    init_random_hap(data->snpfrag,data->snps,data->HAP1);

    // BUILD CONTIGS/CONNECTED COMPONENTS OF GRAPH 
    data->clist = (struct BLOCK*) malloc(sizeof (struct BLOCK)*data->components);
    generate_contigs(data->Flist, data->fragments, data->snpfrag, data->snps,data->components,data->clist);
    fprintf_time(stderr, "fragments %d snps %d component(blocks) %d\n", data->fragments,data->snps,data->components);

    // MAX-CUT OPTIMIZATION
    optimization_using_maxcut(data,MAX_HIC_EM_ITER,MAXITER);

    // POST PROCESSING OF HAPLOTYPES, pass all options as arguments rather than use global variables 
    post_processing(data,SPLIT_BLOCKS);  // only if EA ==1 or SPLIT_BLOCKS ==1 or SKIP_PRUNE ==0

    // UPDATE phased genotype likelihoods using local updates
    if (GENOTYPING ==1) local_optimization(data);

    // FREE DATA STRUCTURES
    if (FILTER_HETS ==1) free_fragmentlist(data->Flist,data->fragments);
    return 0;
}

int main(int argc, char** argv) {
    int i = 0;
    //char* fragfile = NULL; char* fragfile2 = NULL; char* VCFfile = NULL; char* outfile = NULL;
    char fragfile[10000]; char fragfile2[10000];
    strcpy(fragfile, "None");   strcpy(fragfile2, "None"); 
    char VCFfile[10000]; char outfile[10000];
    strcpy(VCFfile, "None"); strcpy(outfile, "None");
    strcpy(HTRANS_DATA_INFILE, "None"); strcpy(HTRANS_DATA_OUTFILE, "None");
    
    // get parameters from command line arguments
    parse_arguments(argc,argv,fragfile,fragfile2,VCFfile,outfile);

    // READ INPUT FILES (Fragments and Variants) into 'data' structure
    DATA data; data.graph_ready = '0';
    if (read_input_files(fragfile,fragfile2,VCFfile,&data) < 1)  return -1;
    else fprintf_time(stderr, "read fragment file and variant file: fragments %d variants %d\n",data.full_fragments,data.snps);
    data.HAP1 = (char*) malloc(data.snps + 1);  // allocate memory for phased haplotype (only het variants)
    data.varlist = calloc(sizeof(PVAR),data.snps); 
    data.MINQ = MINQ;

    esl_flogsum10_init(); // logsum approximation init

    LONG_READS = detect_long_reads(data.full_Flist,data.full_fragments); // detect if data corresponds to long_reads (pacbio/ONT/10X)

    // CALL MAIN FUNCTION
    diploid_haplotyping(&data);
    
    // PRINT OUTPUT FILES
    fprintf_time(stderr, "starting to output phased haplotypes\n");
    print_output_files(&data,VCFfile,outfile);

    // FREE DATA STRUCTURES
    free_memory(data.snpfrag,data.snps,data.clist,data.components);  // only frees the graph-relevant parts of snpfrag
    data.graph_ready = '0';
    free_fragmentlist(data.full_Flist,data.full_fragments);
    free(data.HAP1); 
    free_varlist(data.varlist,data.snps);
    return 0;
}


/*
CODE for likelihood based cut calculation that preserves long reads and operates on fragments rather than edges 

code first implemented 03/02/15 | likelihood based model for initializing two shores of the cut (startnode... secondnode) 

##for each read store 4 floats  P(R | H), P(R | complement(H)) and P(R | H_new), P(R | complement(H_new)) where likelihood is only for nodes added to the cut till now
                                scores[0] scores[1]                scores[2]     scores[3] 

where H_new is new haplotype formed by flipping the phase of vertices in shore2 relative to shore1

score of a variant snpfrag[node].score = log10( P(R|H) + P(R | complement(H)) - log10( P(R|H_new) + P(R | complement(H_new) ) 
 */
#include "common.h"
extern int HIC;

// simple calculation for difference between likelihood of old and new solution based on the Flist[f].scores used for building max-cut

float cut_score(struct fragment* Flist, struct SNPfrags* snpfrag, struct BLOCK* component, char* hap) {
    int i = 0, t = 0, f = 0;
    float oldLL = 0, newLL = 0;
    float scores[4];   // normal scores
    float htscores[4]; // h-trans scores
    float Ln, fL, chimeric_ll, Ln_htrans, normal_ll, htrans_ll;
    // use the 4 values fragment.scores[] to calculate the likelihoods 
    for (i = 0; i < component->frags; i++) {
        f = component->flist[i];
        for (t = 0; t < 4; t++) scores[t] = Flist[f].scores[t];
        //for (t = 0; t < 4; t++) htscores[t] = Flist[f].htscores[t];
        //if (scores[0] > scores[1]) Lo = (scores[0] + log10(1+pow(10,scores[1]-scores[0]))); else Lo = (scores[1] + log10(1+pow(10,scores[0]-scores[1])));
        Ln = addlogs(scores[2], scores[3]);
        //oldLL += Lo; newLL += Ln;
        calculate_fragscore(Flist, f, hap, &fL, &chimeric_ll);
        oldLL += fL; // ready to go with h-trans calculations
        //if (!HIC){
            newLL += Ln;
       // }else{
            // Hi-C reads; account for h-trans
        //    Ln_htrans = addlogs(htscores[2], htscores[3]);
        //    normal_ll = Ln + subtractlogs(0,Flist[i].htrans_prob); // hcis LL times P(hcis)
        //    htrans_ll = Ln_htrans + Flist[i].htrans_prob;          // htrans LL times P(htrans)
            //newLL += addlogs(normal_ll, htrans_ll);
        //}
        //fprintf(stdout,"FRAG SCORES %d %f %f %f \n",f,Lo,Ln,fL);
    }
    if (newLL > oldLL && DEBUG) fprintf(stderr, "component %d old %f new %f %f \n", component->phased, oldLL, newLL, component->MEC);
    if (DEBUG) fprintf(stdout, "FRAG component %d old %f new %f %f \n\n", component->phased, oldLL, newLL, component->MEC);
    //fprintf(stderr,"component %d current LL %f oldLL %f newLL %f \n",component->phased,component->LL,oldLL,newLL);
    return oldLL - newLL;
}

// initialize the scores for initial two nodes
void init_fragment_scores(struct SNPfrags* snpfrag, struct fragment* Flist, char* hap, int startnode, int secondnode) {
    int j = 0, f = 0, t = 0, n = 0, k = 0, node = 0, node1 = 0;
    float prob, prob2;   // prob = P(miscall). prob2 == P(correct) 
    float scores[4];     // a temp variable
    float Lo, Ln;        // old likelihood, new likelihood
    float Lo_htrans, Ln_htrans;
    int htrans_flipped = 0 ;
    
    node1 = startnode;   // handle startnode first
    while (1) {
        //fprintf(stdout,"init scores for %d node \n",node1);
        for (n = 0; n < snpfrag[node1].frags; n++) {
            f = snpfrag[node1].flist[n]; // index into Flist global 
            if (Flist[f].init == '0') continue;
            for (j = 0; j < Flist[f].blocks; j++){ // first find the allele at 'node' in the fragment 'f' 
            
                for (k = 0; k < Flist[f].list[j].len; k++) {
                    if (hap[Flist[f].list[j].offset + k] == '-' || (int) Flist[f].list[j].qv[k] - QVoffset < MINQ) continue;
                    node = Flist[f].list[j].offset + k;
                    htrans_flipped = (Flist[f].mate2_ix != -1 && node >= Flist[f].mate2_ix); // are we flipped due to h-trans?
                    prob = QVoffset - (int) Flist[f].list[j].qv[k];
                    prob /= 10; // log10(e)
                    //prob1 = 1.0 - pow(10,prob); prob2 = log10(prob1);
                    prob2 = Flist[f].list[j].p1[k];

                    if (node == startnode) {
                        if (hap[node] == Flist[f].list[j].hap[k]){
                            Flist[f].scores[0] += prob2;
                            Flist[f].scores[1] += prob;
                            Flist[f].scores[2] += prob2;
                            Flist[f].scores[3] += prob;
                        } else {
                            Flist[f].scores[0] += prob;
                            Flist[f].scores[1] += prob2;
                            Flist[f].scores[2] += prob;
                            Flist[f].scores[3] += prob2;
                        }
                    } else if (node == secondnode){ // probabilities are flipped compared to start node 
                    
                        if (hap[node] == Flist[f].list[j].hap[k]) {
                            Flist[f].scores[0] += prob;
                            Flist[f].scores[1] += prob2;
                            Flist[f].scores[2] += prob;
                            Flist[f].scores[3] += prob2;
                        } else {
                            Flist[f].scores[0] += prob2;
                            Flist[f].scores[1] += prob;
                            Flist[f].scores[2] += prob2;
                            Flist[f].scores[3] += prob;
                        }
                    }
                    
                    // extra calculation for Hi-C h-trans possibility
                    if (HIC){
                        if ((node == startnode && !htrans_flipped)
                          ||(node == secondnode && htrans_flipped)) {
                            if (hap[node] == Flist[f].list[j].hap[k]){
                                Flist[f].htscores[0] += prob2;
                                Flist[f].htscores[1] += prob;
                                Flist[f].htscores[2] += prob2;
                                Flist[f].htscores[3] += prob;
                            } else {
                                Flist[f].htscores[0] += prob;
                                Flist[f].htscores[1] += prob2;
                                Flist[f].htscores[2] += prob;
                                Flist[f].htscores[3] += prob2;
                            }
                        } else if ((node == secondnode && !htrans_flipped)
                                 ||(node == startnode && htrans_flipped)){ // probabilities are flipped compared to start node 

                            if (hap[node] == Flist[f].list[j].hap[k]) {
                                Flist[f].htscores[0] += prob;
                                Flist[f].htscores[1] += prob2;
                                Flist[f].htscores[2] += prob;
                                Flist[f].htscores[3] += prob2;
                            } else {
                                Flist[f].htscores[0] += prob2;
                                Flist[f].htscores[1] += prob;
                                Flist[f].htscores[2] += prob2;
                                Flist[f].htscores[3] += prob;
                            }
                        }
                    }
                }
            }
            
            
            // update score of every node outside 2 shores covered by 'f'
            for (j = 0; j < Flist[f].blocks; j++){
            
                for (k = 0; k < Flist[f].list[j].len; k++) {
                    if (hap[Flist[f].list[j].offset + k] == '-' || (int) Flist[f].list[j].qv[k] - QVoffset < MINQ) continue;
                    node = Flist[f].list[j].offset + k;
                    prob = QVoffset - (int) Flist[f].list[j].qv[k];
                    prob /= 10; // log10(e)
                    //prob1 = 1.0 - pow(10,prob); prob2 = log10(prob1);
                    prob2 = Flist[f].list[j].p1[k];
                    if (node != startnode && node != secondnode) {
                        for (t = 0; t < 4; t++) scores[t] = Flist[f].scores[t];
                        if (hap[node] == Flist[f].list[j].hap[k]) {
                            scores[0] += prob2;
                            scores[1] += prob; // add node to startnode side of cut
                            scores[2] += prob;
                            scores[3] += prob2; // add to other side of cut 
                        } else {
                            scores[0] += prob;
                            scores[1] += prob2; // allele mismatch so flip probability 
                            scores[2] += prob2;
                            scores[3] += prob;
                        }
                        
                        // Lo is likelihood of fragment if SNP is added to 'startnode' and Ln if SNP is added to 'secondnode' 
                        Lo = addlogs(scores[0], scores[1]);
                        Ln = addlogs(scores[2], scores[3]);
                        
                        if (HIC){
                        
                            htrans_flipped = (Flist[f].mate2_ix != -1 && node >= Flist[f].mate2_ix); // are we flipped due to h-trans? 
                            
                            for (t = 0; t < 4; t++) scores[t] = Flist[f].htscores[t];
                            if ((hap[node] == Flist[f].list[j].hap[k] && !htrans_flipped)
                              ||(hap[node] != Flist[f].list[j].hap[k] && htrans_flipped)){
                                scores[0] += prob2;
                                scores[1] += prob; // add node to startnode side of cut
                                scores[2] += prob;
                                scores[3] += prob2; // add to other side of cut 
                            } else if ((hap[node] != Flist[f].list[j].hap[k] && !htrans_flipped)
                                     ||(hap[node] == Flist[f].list[j].hap[k] && htrans_flipped)){
                                scores[0] += prob;
                                scores[1] += prob2; // allele mismatch so flip probability 
                                scores[2] += prob2;
                                scores[3] += prob;
                            }
                            // Lo is likelihood of fragment if SNP is added to 'startnode' and Ln if SNP is added to 'secondnode' 
                            Lo_htrans = addlogs(scores[0], scores[1]);
                            Ln_htrans = addlogs(scores[2], scores[3]);
                            //snpfrag[node].htscore += Lo - Ln;                            
                            
                            // we need to update Lo and Ln so they account for h-trans possibility
                            Lo = addlogs(Lo+subtractlogs(0,Flist[f].htrans_prob), Lo_htrans+Flist[f].htrans_prob);
                            Ln = addlogs(Ln+subtractlogs(0,Flist[f].htrans_prob), Ln_htrans+Flist[f].htrans_prob);                            
                        }
                        
                        snpfrag[node].score += Lo - Ln;
                        
                    }
                }
            }
            Flist[f].init = '0';
        }
        
        // do that stuff for startnode AND secondnode
        if (node1 == startnode) {
            node1 = secondnode;
            if (node1 < 0) break; // if secondnode is not valid, we should exit, otherwise segfault
        } else if (node1 == secondnode) break;
    }
}

// updates fragment scores and variant scores as a result of adding the node 'node_added' to the growing max-cut 

void update_fragment_scores(struct SNPfrags* snpfrag, struct fragment* Flist, char* hap, int startnode, int secondnode, int node_added, struct PHEAP* pheap, int* slist) {
    int j = 0, f = 0, t = 0, n = 0, k = 0, node = 0;
    float prob, prob2;
    float scores[4];
    float htscores[4];
    float Lo, Ln;
    float Lo_htrans, Ln_htrans;
    float f_scores[4];
    float htrans_f_scores[4];
    float oldscore;
    int htrans_flipped = 0;

    for (n = 0; n < snpfrag[node_added].frags; n++){
        f = snpfrag[node_added].flist[n]; // index into Flist global 
        for (t = 0; t < 4; t++) f_scores[t] = Flist[f].scores[t]; // store previous fragment scores before updating
        for (t = 0; t < 4; t++) htrans_f_scores[t] = Flist[f].htscores[t]; // store previous fragment scores before updating

        for (j = 0; j < Flist[f].blocks; j++){ // we only update likelihood for 'node_added' (have to scan through entire fragment), storing fragment as an array could speed this up 
        
            for (k = 0; k < Flist[f].list[j].len; k++){
                if (hap[Flist[f].list[j].offset + k] == '-' || (int) Flist[f].list[j].qv[k] - QVoffset < MINQ) continue;
                node = Flist[f].list[j].offset + k;

                if (node != node_added) continue;

                prob = QVoffset - (int) Flist[f].list[j].qv[k];
                prob /= 10; // log10(e)
                //prob1 = 1.0 - pow(10,prob); prob2 = log10(prob1);
                prob2 = Flist[f].list[j].p1[k];

                if (snpfrag[node].parent == startnode){ // if node is added to 'startnode', original and new likelihoods are updated identically 
                
                    if (hap[node] == Flist[f].list[j].hap[k]){
                        Flist[f].scores[0] += prob2;
                        Flist[f].scores[1] += prob;
                        Flist[f].scores[2] += prob2;
                        Flist[f].scores[3] += prob;
                    } else {
                        Flist[f].scores[0] += prob;
                        Flist[f].scores[1] += prob2;
                        Flist[f].scores[2] += prob;
                        Flist[f].scores[3] += prob2;
                    }
                } else if (snpfrag[node].parent == secondnode){
                    if (hap[node] == Flist[f].list[j].hap[k]){
                        Flist[f].scores[0] += prob;
                        Flist[f].scores[1] += prob2;
                        Flist[f].scores[2] += prob;
                        Flist[f].scores[3] += prob2;
                    } else {
                        Flist[f].scores[0] += prob2;
                        Flist[f].scores[1] += prob;
                        Flist[f].scores[2] += prob2;
                        Flist[f].scores[3] += prob;
                    }
                }
                
                if (HIC){

                    htrans_flipped = (Flist[f].mate2_ix != -1 && node >= Flist[f].mate2_ix); // are we flipped due to h-trans? 

                    if ((snpfrag[node].parent == startnode && !htrans_flipped)
                      ||(snpfrag[node].parent == secondnode && htrans_flipped)){ // if node is added to 'startnode', original and new likelihoods are updated identically 

                        if (hap[node] == Flist[f].list[j].hap[k]){
                            Flist[f].htscores[0] += prob2;
                            Flist[f].htscores[1] += prob;
                            Flist[f].htscores[2] += prob2;
                            Flist[f].htscores[3] += prob;
                        } else {
                            Flist[f].htscores[0] += prob;
                            Flist[f].htscores[1] += prob2;
                            Flist[f].htscores[2] += prob;
                            Flist[f].htscores[3] += prob2;
                        }
                    }else if((snpfrag[node].parent == secondnode && !htrans_flipped)
                           ||(snpfrag[node].parent == startnode && htrans_flipped)){
                        if (hap[node] == Flist[f].list[j].hap[k]){
                            Flist[f].htscores[0] += prob;
                            Flist[f].htscores[1] += prob2;
                            Flist[f].htscores[2] += prob;
                            Flist[f].htscores[3] += prob2;
                        } else {
                            Flist[f].htscores[0] += prob2;
                            Flist[f].htscores[1] += prob;
                            Flist[f].htscores[2] += prob2;
                            Flist[f].htscores[3] += prob;
                        }
                    }
                }
            }
        }
     
        for (j = 0; j < Flist[f].blocks; j++){ // update score of every node outside 2 shores covered by 'f'
        
            for (k = 0; k < Flist[f].list[j].len; k++) {
                if (hap[Flist[f].list[j].offset + k] == '-' || (int) Flist[f].list[j].qv[k] - QVoffset < MINQ) continue;
                node = Flist[f].list[j].offset + k;
                if (snpfrag[node].parent != startnode && snpfrag[node].parent != secondnode && node != node_added) {
                    oldscore = snpfrag[node].score; // store old score 
                    prob = QVoffset - (int) Flist[f].list[j].qv[k];
                    prob /= 10; // log10(e)
                    //prob1 = 1.0 - pow(10,prob); prob2 = log10(prob1);
                    prob2 = Flist[f].list[j].p1[k];
                    for (t = 0; t < 4; t++) scores[t] = f_scores[t];
                    if (hap[node] == Flist[f].list[j].hap[k]) {
                        scores[0] += prob2;
                        scores[1] += prob;
                        scores[2] += prob;
                        scores[3] += prob2;
                    } else {
                        scores[0] += prob;
                        scores[1] += prob2;
                        scores[2] += prob2;
                        scores[3] += prob;
                    }
                    
                    Lo = addlogs(scores[0], scores[1]);
                    Ln = addlogs(scores[2], scores[3]);

                    if (HIC){

                        htrans_flipped = (Flist[f].mate2_ix != -1 && node >= Flist[f].mate2_ix); // are we flipped due to h-trans? 

                        for (t = 0; t < 4; t++) htscores[t] = htrans_f_scores[t];
                        if ((hap[node] == Flist[f].list[j].hap[k] && !htrans_flipped)
                          ||(hap[node] != Flist[f].list[j].hap[k] && htrans_flipped)) {
                            htscores[0] += prob2;
                            htscores[1] += prob;
                            htscores[2] += prob;
                            htscores[3] += prob2;
                        } else {
                            htscores[0] += prob;
                            htscores[1] += prob2;
                            htscores[2] += prob2;
                            htscores[3] += prob;
                        }
                        Lo_htrans = addlogs(htscores[0], htscores[1]);
                        Ln_htrans = addlogs(htscores[2], htscores[3]);
                        
                        // update Lo and Ln for h-trans possibility
                        Lo = addlogs(Lo+subtractlogs(0,Flist[f].htrans_prob), Lo_htrans+Flist[f].htrans_prob);
                        Ln = addlogs(Ln+subtractlogs(0,Flist[f].htrans_prob), Ln_htrans+Flist[f].htrans_prob);
                    }

                    snpfrag[node].score -= Lo - Ln;; // subtract old score for variant 

                    for (t = 0; t < 4; t++) scores[t] = Flist[f].scores[t];
                    if (hap[node] == Flist[f].list[j].hap[k]) {
                        scores[0] += prob2;
                        scores[1] += prob;
                        scores[2] += prob;
                        scores[3] += prob2;
                    } else {
                        scores[0] += prob;
                        scores[1] += prob2;
                        scores[2] += prob2;
                        scores[3] += prob;
                    }
                    
                    Lo = addlogs(scores[0], scores[1]);
                    Ln = addlogs(scores[2], scores[3]);                    

                    if (HIC){
                        for (t = 0; t < 4; t++) htscores[t] = Flist[f].htscores[t];

                        if ((hap[node] == Flist[f].list[j].hap[k] && !htrans_flipped)
                          ||(hap[node] != Flist[f].list[j].hap[k] && htrans_flipped)) {
                            htscores[0] += prob2;
                            htscores[1] += prob;
                            htscores[2] += prob;
                            htscores[3] += prob2;
                        } else {
                            htscores[0] += prob;
                            htscores[1] += prob2;
                            htscores[2] += prob2;
                            htscores[3] += prob;
                        }
                        
                        Lo_htrans = addlogs(htscores[0], htscores[1]);
                        Ln_htrans = addlogs(htscores[2], htscores[3]);
                        
                        // update Lo and Ln for h-trans possibility
                        Lo = addlogs(Lo+subtractlogs(0,Flist[f].htrans_prob), Lo_htrans+Flist[f].htrans_prob);
                        Ln = addlogs(Ln+subtractlogs(0,Flist[f].htrans_prob), Ln_htrans+Flist[f].htrans_prob);
                        
                    }

                    snpfrag[node].score += Lo - Ln; // add new delta LL for variant

                    if (fabsf(oldscore) > fabsf(snpfrag[node].score)){ // score decreased 
                        pmaxHeapify(pheap, snpfrag[node].heaploc, snpfrag, slist);
                    } else pbubbleUp(pheap, snpfrag[node].heaploc, snpfrag, slist);
                }
            }
        }        
    }
}


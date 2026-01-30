#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Library.h"

int SLOTS = 0;

typedef struct find_prev_pair_t{
    genre_t *prev; 
    genre_t *cur;
}find_prev_pair_t;

library_t* create_library(void);
void destroy_library(library_t *lib);

/*GENRES*/
static find_prev_pair_t find_genre_slot(library_t *lib,int gid);
int add_genre(library_t *lib,int gid,const char *name);
genre_t* get_genre(library_t *lib,int gid);

/*MEMBERS*/
int add_member(library_t *lib,int sid,const char *name);
member_t* get_member(library_t *lib,int sid);

/*BOOKS*/
int add_book(library_t *lib,int bid,int gid,const char *title);
book_t* get_book(library_t *lib,int gid);

/*PRINTS*/
void print_genre_books(library_t *lib,int gid);
void print_member_loans(library_t *lib,int sid);



library_t* create_library(void){
    library_t *lib = malloc(sizeof(*lib));
    if (!lib) { perror("malloc"); exit(1); }

    /* Phase 1 */
    lib->genres  = NULL;
    lib->members = NULL;
    lib->books   = NULL;

    /* Phase 2 */
    lib->recommendations = malloc(sizeof(RecHeap));
    if (!lib->recommendations) { perror("malloc"); exit(1); }
    rec_init(lib->recommendations);   /* max-heap προτάσεων */
    lib->activity = NULL;             /* head λίστας activity */

    return lib;
}

static void free_genre_display(genre_t *g) {
    if (g->display) { 
        free(g->display); 
        g->display = NULL; 
    }
    g->display_count = 0;
}


static void free_all_genres(genre_t *head){
    while(head){
        genre_t *nextg = head->next_by_gid;
        free_genre_display(head);
        nextg = head;
    }
}

static void free_all_members(member_t *head){
    while (head){
        member_t *nextm = head->next_all;
        loan_t *ln = head->loans_head.next;
        while (ln){
            loan_t *nx = ln->next;
            free(ln);
            ln = nx;
        }
    }
}

static void free_all_books(book_t *head){
    while (head){
        book_t *nx = head->next_global;
        free(head);
        head = nx;
    }
}

static void free_avl(BookNode *r) {
    if (!r) return;
    free_avl(r->lc);
    free_avl(r->rc);
    free(r->title);
    free(r);
}

void destroy_library(library_t *lib){
    if (!lib) return;

    for (genre_t *g = lib->genres; g; g = g->next_by_gid) {
        void free_avl(BookNode *r) {
            if (!r) return;
            free_avl(r->lc);
            free_avl(r->rc);
            free(r->title);
            free(r);
        }
        free_avl(g->bookIndex);
        g->bookIndex = NULL;
    }

    while (lib->activity) {
        MemberActivity *n = lib->activity->next;
        free(lib->activity);
        lib->activity = n;
    }

    free(lib->recommendations);

    free_all_genres(lib->genres);
    free_all_members(lib->members);
    free(lib);
}

static genre_t* create_genre_node(int gid, const char *name){
    genre_t *g = (genre_t*)malloc(sizeof(genre_t));
    if (!g) { perror("malloc"); exit(1); }
    g->gid = gid;
    strncpy(g->name, name, NAME_MAX); g->name[NAME_MAX-1] = '\0';
    
    g->book_sentinel.prev_in_genre = &g->book_sentinel;
    g->book_sentinel.next_in_genre = &g->book_sentinel;
    g->book_sentinel.gid = gid;

    g->display = NULL;
    g->display_count = 0;
    g->next_by_gid = NULL;
    return g;
}

static find_prev_pair_t find_genre_slot(library_t *lib, int gid) {
    find_prev_pair_t r = { .prev = NULL, .cur = lib->genres };
    while (r.cur && r.cur->gid < gid) { 
        r.prev = r.cur; 
        r.cur = r.cur->next_by_gid; 
    }
    return r;
}

int add_genre(library_t *lib,int gid,const char *name){
    if (!lib) return 0; 
    find_prev_pair_t slot = find_genre_slot(lib, gid);
    if (slot.cur && slot.cur->gid == gid) return 0;

    genre_t *g = create_genre_node(gid, name);
    if (!slot.prev) { 
        g->next_by_gid = lib->genres; 
        lib->genres = g; 
    } else { 
        g->next_by_gid = slot.prev->next_by_gid; 
        slot.prev->next_by_gid = g; 
    }
    return 1;
}

genre_t* get_genre(library_t *lib, int gid) {
    find_prev_pair_t slot = find_genre_slot(lib, gid);
    if (slot.cur && slot.cur->gid == gid) return slot.cur;
    return NULL;
}

static member_t* create_member_node(int sid,const char *name){
    member_t *m = (member_t*)malloc(sizeof(member_t));
    if (!m) { perror("malloc"); exit(1); }
    m->sid = sid;
    strncpy(m->name, name, NAME_MAX); m->name[NAME_MAX-1] = '\0';

    m->loans_head.sid = sid; m->loans_head.bid = -1; m->loans_head.next = NULL;
    m->prev_all = NULL; m->next_all = NULL;
    return m;
}

int add_member(library_t *lib, int sid, const char *name) {
    for (member_t *m = lib->members; m; m = m->next_all)
        if (m->sid == sid) return 0; /* IGNORED */

    member_t *newm = create_member_node(sid, name);
    newm->next_all = lib->members;
    lib->members   = newm;

    activity_attach_member(lib, newm); 

    return 1; /* DONE */
}

member_t* get_member(library_t *lib, int sid) {
    for (member_t *m = lib->members; m; m = m->next_all) if (m->sid == sid) return m;
    return NULL;
}

static book_t* create_book_node(int bid, int gid, const char *title) {
    book_t *b = malloc(sizeof(*b));
    if (!b) {
        perror("malloc");
        exit(1);
    }

    b->bid = bid;
    b->gid = gid;

    strncpy(b->title, title, TITLE_MAX - 1);
    b->title[TITLE_MAX - 1] = '\0';

    b->sum_scores = 0;
    b->n_reviews  = 0;
    b->lost_flag  = 0;

    b->next_in_genre = NULL;
    b->prev_in_genre = NULL;

    /* Phase 2 */
    b->heap_pos = -1;

    return b;
}

book_t* get_book(library_t *lib, int bid) {
    for (book_t *b = lib->books; b; b = b->next_global) if (b->bid == bid) return b;
    return NULL;
}

static void genre_insert_sorted_by_avg_desc(genre_t *g, book_t *b) {
    book_t *sent = &g->book_sentinel;
    book_t *it = sent->next_in_genre;
    while (it != sent) {
    if (it->avg < b->avg) break;
    if (it->avg == b->avg && it->bid > b->bid) break;
    it = it->next_in_genre;
    }
    b->next_in_genre = it;
    b->prev_in_genre = it->prev_in_genre;
    it->prev_in_genre->next_in_genre = b;
    it->prev_in_genre = b;
}

int add_book(library_t *lib,int bid,int gid,const char *title){
    genre_t *g = get_genre(lib, gid);
    if (!g) return 0;  /* IGNORED: άκυρο gid */

    /*Phase 1: δημιουργία & εισαγωγή στη DLL του genre*/
    book_t *b = create_book_node(bid, gid, title);

    book_t *sent = &g->book_sentinel;
    if (!sent->next_in_genre) {
        sent->next_in_genre = sent;
        sent->prev_in_genre = sent;
    }

    b->next_in_genre = sent->next_in_genre;
    b->prev_in_genre = sent;
    sent->next_in_genre->prev_in_genre = b;
    sent->next_in_genre = b;

    /*Phase 2: AVL index & heap init*/
    g->bookIndex = avl_insert_title(g->bookIndex, title, b);
    b->heap_pos  = -1;

    return 1; /* DONE */
}

void print_genre_books(library_t *lib, int gid) {
    genre_t *g = get_genre(lib, gid);
    if (!g) { printf("IGNORED\n"); return; }
    book_t *sent = &g->book_sentinel;
    for (book_t *b = sent->next_in_genre; b != sent; b = b->next_in_genre) {
        printf("%d, %d\n", b->bid, b->avg);
    }
}

void print_member_loans(library_t *lib, int sid) {
    member_t *m = get_member(lib, sid);
    if (!m) { printf("Loans:\n"); return; }
    printf("Loans:\n");
    for (loan_t *ln = m->loans_head.next; ln; ln = ln->next) {
        printf("%d\n", ln->bid);
    }
}    


int cmd_loan(library_t *lib, int sid, int bid) {
    member_t *m = get_member(lib, sid);
    book_t   *b = get_book(lib, bid);
    if (!m || !b) return 0; /* IGNORED */

    /* Phase 1: έλεγχος διπλού δανεισμού*/
    for (loan_t *cur = m->loans_head.next; cur; cur = cur->next)
        if (cur->bid == bid) return 0; 

    /*Phase 1: εισαγωγή στη λίστα δανεισμών του μέλους */
    loan_t *ln = malloc(sizeof(*ln));
    if (!ln) { perror("malloc"); exit(1); }
    ln->sid = sid;
    ln->bid = bid;
    ln->next = m->loans_head.next;
    m->loans_head.next = ln;

    /*Phase 2: activity ενημέρωση*/
    activity_on_loan(lib, sid);  

    return 1; /* DONE */ 
}


int cmd_return(library_t *lib, int sid, int bid, int rating) {
    member_t *m = get_member(lib, sid);
    book_t   *b = get_book(lib, bid);
    if (!m || !b) return 0; /* IGNORED */

    loan_t *prev = &m->loans_head;
    loan_t *cur  = m->loans_head.next;
    while (cur && cur->bid != bid) { prev = cur; cur = cur->next; }
    if (!cur) return 0; 
    prev->next = cur->next;
    free(cur);

    /*Phase 1: ενημέρωση στατιστικών βιβλίου*/
    if (rating >= 0) {
        b->n_reviews++;
        b->sum_scores += rating;
    }

    /*Phase 2: activity & recommendation heap*/
    if (rating >= 0) {
        activity_on_review(lib, sid, rating);
        rec_on_score_change(lib->recommendations, b);
    }
    
    return 1; /* DONE */

}


int cmd_distribute(library_t *lib) {
    if (!lib) return 0;

    int G = 0;
    for (genre_t *g = lib->genres; g; g = g->next_by_gid) G++;

    if (G == 0) {
        for (genre_t *g = lib->genres; g; g = g->next_by_gid) {
            free_genre_display(g);
        }
        return 1; // DONE
    }

    typedef struct {
        genre_t *g;
        long points;
        long seats;
        long rem;
    } Row;

    Row *rows = (Row*)calloc(G, sizeof(Row));
    if (!rows) { perror("calloc"); return 0; }

    int i = 0;
    long total = 0;
    for (genre_t *g = lib->genres; g; g = g->next_by_gid) {
        rows[i].g = g;
        
        long pts = 0;
        book_t *sent = &g->book_sentinel;
        for (book_t *b = sent->next_in_genre; b != sent; b = b->next_in_genre) {
            if (b->lost_flag == 0 && b->n_reviews > 0) {
                pts += b->sum_scores;
            }
        }
        rows[i].points = pts;
        total += pts;
        i++;
    }

    
    for (genre_t *g = lib->genres; g; g = g->next_by_gid) {
        free_genre_display(g);
    }

    if (SLOTS == 0 || total == 0) {
        free(rows);
        return 1; 
    }

    long quota = total / SLOTS; 
    long sum_seats = 0;

    for (int k = 0; k < G; ++k) {
        if (quota == 0) {
            rows[k].seats = 0;
            rows[k].rem   = rows[k].points; 
        } else {
            rows[k].seats = rows[k].points / quota;
            rows[k].rem   = rows[k].points - rows[k].seats * quota;
        }
        sum_seats += rows[k].seats;
    }

    long leftover = SLOTS - sum_seats;
    if (leftover < 0) leftover = 0;

    for (int a = 0; a < G; ++a) {
        int best = a;
        for (int b = a + 1; b < G; ++b) {
            if (rows[b].rem > rows[best].rem) best = b;
            else if (rows[b].rem == rows[best].rem) {
                if (rows[b].g->gid < rows[best].g->gid) best = b; 
            }
        }
        if (best != a) {
            Row tmp = rows[a]; rows[a] = rows[best]; rows[best] = tmp;
        }
    }

    for (int k = 0; k < G && leftover > 0; ++k) {
        rows[k].seats += 1;
        leftover--;
    }

    for (int k = 0; k < G; ++k) {
        genre_t *g = rows[k].g;
        long need = rows[k].seats;

        if (need <= 0) {
            g->display = NULL;
            g->display_count = 0;
            continue;
        }

        g->display = (book_t**)calloc((size_t)need, sizeof(book_t*));
        if (!g->display) { perror("calloc"); free(rows); return 0; }

        long placed = 0;
        book_t *sent = &g->book_sentinel;
        for (book_t *b = sent->next_in_genre; b != sent && placed < need; b = b->next_in_genre) {
            if (b->lost_flag == 0) {
                g->display[placed++] = b;
            }
        }
        g->display_count = (int)placed;
    }

    free(rows);
    return 1;
    
}


void print_display(library_t *lib) {
    printf("Display:\n");
    if (!lib){
        printf("(empty)\n");
        return;
    }
    int any = 0;
    for (genre_t *g = lib->genres; g; g = g-> next_by_gid ){
        if (g->display && g->display_count > 0) {
            any = 1;
            printf("%d:\n", g->gid);
            for (int i = 0; i < g->display_count; ++i) {
                book_t *b = g->display[i];
                if (b){
                    printf("%d, %d\n", b->bid, b->avg);
                }
            }
        }            
    }

    if (!any){
        printf("(empty)");
    }
    
}


void print_stats(library_t *lib) {
    if (!lib) {
        printf("SLOTS=0\n");
        return;
    }

    printf("SLOTS=%d\n", SLOTS);
    for (genre_t *g = lib->genres; g; g = g->next_by_gid) {
        long points = 0;

        book_t *sent = &g->book_sentinel;
        for (book_t *b = sent->next_in_genre; b != sent; b = b->next_in_genre) {
            if (b->lost_flag == 0 && b->n_reviews > 0) {
                points += b->sum_scores;
            }
        }

        printf("%d: points=%ld\n", g->gid, points);
    }
    
}



/* ---- AVL helpers ---- */
static int h(BookNode *n){ return n ? n->height : 0; }
static int mx(int a,int b){ return a>b?a:b; }

static BookNode* rotate_right(BookNode *y){
    BookNode *x = y->lc, *T2 = x->rc;
    x->rc = y; y->lc = T2;
    y->height = 1 + mx(h(y->lc), h(y->rc));
    x->height = 1 + mx(h(x->lc), h(x->rc));
    return x;
}
static BookNode* rotate_left(BookNode *x){
    BookNode *y = x->rc, *T2 = y->lc;
    y->lc = x; x->rc = T2;
    x->height = 1 + mx(h(x->lc), h(x->rc));
    y->height = 1 + mx(h(y->lc), h(y->rc));
    return y;
}
static int balance(BookNode *n){ return n ? h(n->lc)-h(n->rc) : 0; }

BookNode* avl_insert_title(BookNode *root, const char *title, book_t *b){
    if (!root){
        BookNode *nn = malloc(sizeof(*nn));
        if(!nn){ perror("malloc"); exit(1); }
        nn->title = strdup(title);
        nn->book  = b;
        nn->lc = nn->rc = NULL;
        nn->height = 1;
        return nn;
    }
    int cmp = strcmp(title, root->title);
    if (cmp == 0) {
        return root;
    } else if (cmp < 0) root->lc = avl_insert_title(root->lc, title, b);
    else                root->rc = avl_insert_title(root->rc, title, b);

    root->height = 1 + mx(h(root->lc), h(root->rc));
    int bf = balance(root);

    if (bf > 1 && strcmp(title, root->lc->title) < 0)  return rotate_right(root);
    if (bf < -1 && strcmp(title, root->rc->title) > 0) return rotate_left(root);
    if (bf > 1 && strcmp(title, root->lc->title) > 0){ root->lc = rotate_left(root->lc); return rotate_right(root); }
    if (bf < -1 && strcmp(title, root->rc->title) < 0){ root->rc = rotate_right(root->rc); return rotate_left(root); }
    return root;
}

static BookNode* avl_min_node(BookNode *n){ while(n && n->lc) n=n->lc; return n; }

BookNode* avl_delete_title(BookNode *root, const char *title){
    if (!root) return NULL;
    int cmp = strcmp(title, root->title);
    if (cmp < 0) root->lc = avl_delete_title(root->lc, title);
    else if (cmp > 0) root->rc = avl_delete_title(root->rc, title);
    else {
        if (!root->lc || !root->rc){
            BookNode *t = root->lc ? root->lc : root->rc;
            if (!t) { free(root->title); free(root); return NULL; }
            else {
                BookNode tmp = *t; 
                free(root->title); *root = tmp; free(t);
            }
        } else {
            BookNode *m = avl_min_node(root->rc);
            free(root->title);
            root->title = strdup(m->title);
            root->book  = m->book;
            root->rc = avl_delete_title(root->rc, m->title);
        }
    }
    root->height = 1 + mx(h(root->lc), h(root->rc));
    int bf = balance(root);
    if (bf > 1 && balance(root->lc) >= 0) return rotate_right(root);
    if (bf > 1 && balance(root->lc) < 0) { root->lc=rotate_left(root->lc); return rotate_right(root); }
    if (bf < -1 && balance(root->rc) <= 0) return rotate_left(root);
    if (bf < -1 && balance(root->rc) > 0)  { root->rc=rotate_right(root->rc); return rotate_left(root); }
    return root;
}

book_t* avl_find_title(BookNode *root, const char *title){
    while(root){
        int c = strcmp(title, root->title);
        if (c==0) return root->book;
        root = (c<0)? root->lc : root->rc;
    }
    return NULL;
}




static double book_avg(const book_t *b){
     return (b->n_reviews > 0) ? ((double)b->sum_scores / b->n_reviews) : 0.0;
}

static int rec_better(book_t *a, book_t *b){
    double aa = book_avg(a), bb = book_avg(b);
    if (aa > bb) return 1;
    if (aa < bb) return 0;
    return a->bid < b->bid; /* tie-break: μικρότερο bid κερδίζει */
}

void rec_init(RecHeap *rh){ rh->size = 0; }

static void swap_nodes(RecHeap *rh, int i, int j){
    book_t *ti = rh->heap[i], *tj = rh->heap[j];
    rh->heap[i]=tj; rh->heap[j]=ti;
    if (tj) tj->heap_pos = i;
    if (ti) ti->heap_pos = j;
}
static void swim(RecHeap *rh, int i){
    while(i>0){
        int p=(i-1)/2;
        if (rec_better(rh->heap[i], rh->heap[p])){
            swap_nodes(rh,i,p); i=p;
        } else break;
    }
}
static void sink(RecHeap *rh, int i){
    for(;;){
        int l=2*i+1, r=2*i+2, best=i;
        if (l<rh->size && rec_better(rh->heap[l], rh->heap[best])) best=l;
        if (r<rh->size && rec_better(rh->heap[r], rh->heap[best])) best=r;
        if (best==i) break;
        swap_nodes(rh,i,best); i=best;
    }
}

static void sink_tmp_rec(book_t **tmp, int size, int i){
    for(;;){
        int l = 2*i + 1, r = 2*i + 2, best = i;
        if (l < size && rec_better(tmp[l], tmp[best])) best = l;
        if (r < size && rec_better(tmp[r], tmp[best])) best = r;
        if (best == i) break;
        book_t *t = tmp[i]; tmp[i] = tmp[best]; tmp[best] = t;
        i = best;
    }
}


static int find_worst_idx(const RecHeap *rh){
    if (rh->size==0) return -1;
    int w=0;
    for(int i=1;i<rh->size;i++){
        if (!rec_better(rh->heap[i], rh->heap[w])) {
            /* worse or equal but με μεγαλύτερο bid */
            double ai=book_avg(rh->heap[i]), aw=book_avg(rh->heap[w]);
            if (ai < aw || (ai==aw && rh->heap[i]->bid > rh->heap[w]->bid)) w=i;
        }
    }
    return w;
}

void rec_on_score_change(RecHeap *rh, book_t *b){
    if (!rh || !b) return;
    if (b->heap_pos >= 0) {
        int i = b->heap_pos;
        swim(rh,i); sink(rh,i);
        return;
    }
    if (rh->size < REC_CAP){
        rh->heap[rh->size] = b;
        b->heap_pos = rh->size;
        rh->size++;
        swim(rh, b->heap_pos);
    } else {
        int worst = find_worst_idx(rh);
        if (worst>=0 && rec_better(b, rh->heap[worst])){
            rh->heap[worst]->heap_pos = -1;
            rh->heap[worst] = b;
            b->heap_pos = worst;
            swim(rh,worst); sink(rh,worst);
        }
    }
}

void rec_remove(RecHeap *rh, book_t *b){
    if (!rh || !b || b->heap_pos<0) return;
    int i = b->heap_pos;
    int last = rh->size-1;
    swap_nodes(rh, i, last);
    rh->heap[last]->heap_pos = -1;
    rh->heap[last] = NULL;
    rh->size--;
    if (i < rh->size){ swim(rh,i); sink(rh,i); }
}

void rec_print_top(const RecHeap *rh, int k){
    printf("Top Books:\n");
    if (!rh || rh->size == 0 || k <= 0) return;

    book_t *tmp[REC_CAP];
    int size = rh->size;
    for (int i = 0; i < size; ++i) tmp[i] = rh->heap[i];

    /* heapify O(n) */
    for (int i = size/2 - 1; i >= 0; --i) sink_tmp_rec(tmp, size, i);

    for (int c = 0; c < k && size > 0; ++c){
        book_t *top = tmp[0];
        printf("%d \"%s\" avg=%.2f\n", top->bid, top->title, book_avg(top));
        tmp[0] = tmp[size-1];
        size--;
        sink_tmp_rec(tmp, size, 0);
    }
}






MemberActivity* activity_attach_member(library_t *lib, member_t *m){
    MemberActivity *a = malloc(sizeof(*a));
    if (!a){ perror("malloc"); exit(1); }
    a->sid = m->sid;
    a->loans_count = 0;
    a->reviews_count = 0;
    a->score_sum = 0;
    a->next = lib->activity;
    lib->activity = a;
    m->activity = a;
    return a;
}
void activity_on_loan(library_t *lib, int sid){
    for(MemberActivity *p=lib->activity; p; p=p->next)
        if (p->sid==sid){ p->loans_count++; return; }
}
void activity_on_review(library_t *lib, int sid, int rating){
    for(MemberActivity *p=lib->activity; p; p=p->next)
        if (p->sid==sid){ p->reviews_count++; p->score_sum += rating; return; }
}
void activity_print_all(library_t *lib){
    printf("Active Members:\n");
    for(MemberActivity *p=lib->activity; p; p=p->next){
        /* βρες όνομα μέλους για εκτύπωση */
        member_t *m = get_member(lib, p->sid);
        printf("%d %s loans=%d reviews=%d\n",
               p->sid, m? m->name : "(unknown)",
               p->loans_count, p->reviews_count);
    }
}





int cmd_find_title(library_t *lib, const char *title){
    /* αναζήτηση σε όλα τα genres (ή αν ξέρεις gid, στο συγκεκριμένο) */
    for (genre_t *g=lib->genres; g; g=g->next_by_gid){
        book_t *b = avl_find_title(g->bookIndex, title);
        if (b){
            printf("Book %d \"%s\" avg=%.2f\n", b->bid, b->title, book_avg(b));
            return 1;
        }
    }
    printf("NOT FOUND\n");
    return 0;
}

int cmd_top_k(library_t *lib, int k){
    rec_print_top(lib->recommendations, k);
    return 1;
}

int cmd_active_members(library_t *lib){
    activity_print_all(lib);
    return 1;
}

int cmd_update_title(library_t *lib, int bid, const char *new_title){
    book_t *b = get_book(lib, bid);
    if (!b) {
        printf("IGNORED\n");
        return 0;
    }
    genre_t *g = get_genre(lib, b->gid);
    if (!g) {
        printf("IGNORED\n");
        return 0;
    }

    /* Βγάλε το παλιό title από το AVL index */
    g->bookIndex = avl_delete_title(g->bookIndex, b->title);

    /* Ενημέρωσε το title μέσα στο array του book_t */
    strncpy(b->title, new_title, TITLE_MAX - 1);
    b->title[TITLE_MAX - 1] = '\0';

    /* Ξαναβάλε το βιβλίο στο AVL με το νέο title */
    g->bookIndex = avl_insert_title(g->bookIndex, b->title, b);

    printf("DONE\n");
    return 1;
}

int cmd_stats(library_t *lib){
    if (!lib) {
        printf("BOOKS=0 MEMBERS=0 LOANS=0 AVG=0.00\n");
        return 1;
    }

    int   books = 0;
    int   members = 0;
    int   active_loans = 0;
    long  sum_scores = 0;
    int   num_reviews = 0;

    /* Περνάμε από όλα τα genres και τα βιβλία τους */
    for (genre_t *g = lib->genres; g; g = g->next_by_gid) {
        book_t *sent = &g->book_sentinel;
        for (book_t *b = sent->next_in_genre; b != sent; b = b->next_in_genre) {
            books++;
            if (b->lost_flag == 0 && b->n_reviews > 0) {
                sum_scores  += b->sum_scores;
                num_reviews += b->n_reviews;
            }
        }
    }

    /* Περνάμε από όλα τα μέλη και μετράμε ενεργούς δανεισμούς */
    for (member_t *m = lib->members; m; m = m->next_all) {
        members++;
        for (loan_t *ln = m->loans_head.next; ln; ln = ln->next)
            active_loans++;
    }

    double avg = (num_reviews > 0)
                 ? (double)sum_scores / num_reviews
                 : 0.0;

    printf("BOOKS=%d MEMBERS=%d LOANS=%d AVG=%.2f\n",
           books, members, active_loans, avg);

    return 1;
}

int cmd_bf(library_t *lib){
    destroy_library(lib);
    printf("DONE\n");
    return 1;
}











































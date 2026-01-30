#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Library.h"

library_t* create_library(void);
void destroy_library(library_t *lib);
int add_genre(library_t *lib, int gid, const char *name);
int add_member(library_t *lib, int sid, const char *name);
int add_book(library_t *lib, int bid, int gid, const char *title);
void print_genre_books(library_t *lib, int gid);
void print_member_loans(library_t *lib, int sid);
int cmd_loan(library_t *lib, int sid, int bid);
int cmd_return(library_t *lib, int sid, int bid, int score, int score_is_na, const char *status);
int cmd_distribute(library_t *lib);
void print_display(library_t *lib);
void print_stats(library_t *lib);

extern int SLOTS;

static void skip_rest_of_line(FILE *fp) {
    int c;
    while ((c = fgetc(fp)) != '\n' && c != EOF) {
        /* skip */
    }
}


int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <input-file>\n", argv[0]);
        return 1;
    }
    FILE *fp = fopen(argv[1], "r");
    if (!fp) { perror("open"); return 1; }
    library_t *lib = create_library();

    char cmd[8];
    while (fscanf(fp, "%7s", cmd) == 1) {
        if (strcmp(cmd, "S") == 0) {
            if (fscanf(fp, "%d", &SLOTS) == 1){
                printf("DONE\n");
            }else{
                printf("IGNORED\n");
            }
            skip_rest_of_line(fp);
        }else if(strcmp(cmd, "G") == 0){
            int gid; char name[NAME_MAX];
            if (fscanf(fp, "%d %63s", &gid, name) == 2 && add_genre(lib, gid, name)) {
                printf("DONE\n");
            }else{ 
                printf("IGNORED\n");
            }
                skip_rest_of_line(fp);
        }else if(strcmp(cmd, "BK") == 0){
            int bid, gid; char title[TITLE_MAX];
            if (fscanf(fp, "%d %d", &bid, &gid) != 2) { 
                printf("IGNORED\n"); 
                skip_rest_of_line(fp); 
                continue; 
            }
            int c; 
            while ((c = fgetc(fp)) == ' ') {/*skip spaces*/}
            if (c != '"') { 
                printf("IGNORED\n"); 
                skip_rest_of_line(fp); 
                continue; 
            }
            size_t k = 0; 
            while ((c = fgetc(fp)) != EOF && c != '"' && k < TITLE_MAX-1) { 
                title[k++] = (char)c; 
            }
            title[k] = '\0';
            printf(add_book(lib, bid, gid, title) ? "DONE\n" : "IGNORED\n");
            skip_rest_of_line(fp);
        }else if(strcmp(cmd, "M") == 0){
            int sid; char name[NAME_MAX];
            if (fscanf(fp, "%d %63s", &sid, name) == 2 && add_member(lib, sid, name)){
                printf("DONE\n");
            }else{ 
                printf("IGNORED\n");
                skip_rest_of_line(fp);
            }    
        }else if(strcmp(cmd, "L") == 0){
            int sid, bid;
            if (fscanf(fp, "%d %d", &sid, &bid) == 2 && cmd_loan(lib, sid, bid)){ 
                printf("DONE\n");
            }else{ 
                printf("IGNORED\n");
            }    
            skip_rest_of_line(fp);
        }else if(strcmp(cmd, "R") == 0){
            int sid, bid; char scoretok[8]; char status[8];
            if (fscanf(fp, "%d %d %7s %7s", &sid, &bid, scoretok, status) == 4) {
                int score_is_na = (strcmp(scoretok, "NA") == 0);
                int score = score_is_na ? 0 : atoi(scoretok);
                printf(cmd_return(lib, sid, bid, score, score_is_na, status) ? "DONE\n" : "IGNORED\n");
            } else {
                printf("IGNORED\n");
            }
            skip_rest_of_line(fp);
        }else if(strcmp(cmd, "PG") == 0){
            int gid; 
            if (fscanf(fp, "%d", &gid) == 1) {
                print_genre_books(lib, gid); 
            }else{ 
                printf("IGNORED\n");
            }    
            skip_rest_of_line(fp);
        }else if(strcmp(cmd, "PM") == 0){
            int sid; 
            if (fscanf(fp, "%d", &sid) == 1){ 
                print_member_loans(lib, sid); 
            }else { 
                printf("Loans:\n"); 
            }
            skip_rest_of_line(fp);
        }else if(strcmp(cmd, "PD") == 0){
            print_display(lib);
            skip_rest_of_line(fp);
        }else if(strcmp(cmd, "D") == 0){
            printf(cmd_distribute(lib) ? "DONE\n" : "IGNORED\n");
            skip_rest_of_line(fp);
        }else if(strcmp(cmd, "PS") == 0){
            print_stats(lib);
            skip_rest_of_line(fp);
        }else if (strcmp(cmd, "F") == 0){
            /* τίτλος εντός εισαγωγικών */
            int ch; while((ch=fgetc(fp))==' '){} /* skip spaces */
            if (ch!='"'){ printf("IGNORED\n"); skip_rest_of_line(fp); }
            else {
                char title[TITLE_MAX]; int k=0;
                while((ch=fgetc(fp))!=EOF && ch!='"' && k<TITLE_MAX-1) title[k++]=(char)ch;
                title[k]='\0';
                printf("%s\n", cmd_find_title(lib, title)? "DONE":"IGNORED");
                skip_rest_of_line(fp);
            }
        }
        else if (strcmp(cmd, "TOP") == 0){
            int k; if (fscanf(fp, "%d", &k)==1){ cmd_top_k(lib,k); }
            else printf("IGNORED\n");
            skip_rest_of_line(fp);
        }
        else if (strcmp(cmd, "AM") == 0){
            cmd_active_members(lib);
            skip_rest_of_line(fp);
        }
        else if (strcmp(cmd, "U") == 0){
            int bid; int ch;
            if (fscanf(fp, "%d", &bid)!=1){ printf("IGNORED\n"); skip_rest_of_line(fp); }
            else {
                while((ch=fgetc(fp))==' '){} /* μέχρι το " */
                if (ch!='"'){ printf("IGNORED\n"); skip_rest_of_line(fp); }
                else {
                    char nt[TITLE_MAX]; int k=0;
                    while((ch=fgetc(fp))!=EOF && ch!='"' && k<TITLE_MAX-1) nt[k++]=(char)ch;
                    nt[k]='\0';
                    cmd_update_title(lib,bid,nt); /* εκτυπώνει DONE/IGNORED μέσα της */
                    skip_rest_of_line(fp);
                }
            }
        }
        else if (strcmp(cmd, "X") == 0){
            cmd_stats(lib);
            skip_rest_of_line(fp);
        }
        else if (strcmp(cmd, "BF") == 0){
            cmd_bf(lib);
            /* προσοχή: από εδώ και πέρα lib δεν ισχύει—αν συνεχίζεις διάβασμα, φτιάξε νέο create_library() */
            skip_rest_of_line(fp);
        }
        else {
            skip_rest_of_line(fp);
            printf("IGNORED\n");
        }

    }

    destroy_library(lib);
    fclose(fp);
    return 0;

}


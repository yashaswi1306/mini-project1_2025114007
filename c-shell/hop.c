#include "part_b.h"

hop_command *head=NULL;
hop_command *tail=NULL;

char prev_cwd[PATH_SIZE]=""; //prev dir


void get_hist(char *filename)
{
    char*home=getenv("HOME");
    if(home==NULL){
        strcpy(filename,"\0");
    }
    else{
        snprintf(filename,PATH_SIZE,"%s/.hop_prev",home);
    }
}

//new node
void insert_node(char *path,int visit)
{
    hop_command *newnode=malloc(sizeof(hop_command));
    if(newnode==NULL)
    {
        return;
    }

    strncpy(newnode->path,path,PATH_SIZE-1);
    newnode->hits=visit;
    newnode->next=NULL;
    newnode->path[PATH_SIZE-1]=0;
    
    if(tail==NULL)
    {
        head=tail=newnode;
    }
    else
    {
        tail->next=newnode;
        tail=newnode;
    }
    return;
}

//del list
void clear_list()
{
    hop_command *cur = head;
    while(cur != NULL)
    {
        hop_command *next = cur->next;
        free(cur);
        cur = next;
    }
    head = NULL;
    tail = NULL;
}


// to load the dir history
void load_cache()
{
    // int count=0;

    char filename[PATH_SIZE];
    get_hist(filename);

    if(strcmp(filename,"\0")==0){
        return;
    }

    FILE *f=fopen(filename,"r");

    if(f==NULL){
        return;
    }
    clear_list(); 
    
    int visit;
    char path[PATH_SIZE];

    while(fscanf(f,"%d %[^\n]",&visit,path)==2)
    {
        insert_node(path,visit);
    }
    fclose(f);
}



hop_command *find_match_path(char* path)
{
    hop_command *temp=head;
    while(temp!=NULL)
    {
        if(strcmp(temp->path,path)==0)
        {
            return temp;
        }
        temp=temp->next;
    }
    return NULL;
}

//move recent to fron of linked list (basic cpro stuff hai)
void move_to_front(hop_command*temp)
{
    temp->hits++;
    if(temp==head)
    {
        return;
    }
    hop_command* prev=NULL;
    hop_command* curr=head;
    
    while(curr!=NULL && curr!=temp)
    {
        prev=curr;
        curr=curr->next;
    }

    if(prev!=NULL)
    {
        prev->next=curr->next;
    }

    if(curr==tail)
    {
        tail=prev;
    }
    
    curr->next=head;
    head=curr;
    
    return;
}
//del_node **************SEG FAULT HUA TOH CHECK HERE FIRST!!!!!
void del_node(hop_command *temp)
{
    hop_command* prev=NULL;
    hop_command* curr=head;

    while(curr)
    {
        if(curr==temp)
        {
            if(prev!=NULL)
            {
                prev->next=curr->next;
            }
            else
            {
                head=curr->next;
            }
            if(curr==tail)
            {
                tail=prev;
            }
            free(curr);
            return;
        }
        prev=curr;
        curr=curr->next;
    }
}



// save to dir hist
void save_cache()
{
    char filename[PATH_SIZE];
    get_hist(filename);

    if(strcmp(filename,"\0")==0)
    {
        return;
    }

    FILE *f=fopen(filename,"w");
    
    if(f==NULL)
    {
        return;
    }

    hop_command *new=head;
    while(new!=NULL)
    {
        fprintf(f,"%d %s\n",new->hits,new->path);
        new=new->next;
    }
    fclose(f);
}

// record hop (frequency order)
void update_cache(char *path)
{
    hop_command *flag=find_match_path(path);
    if(flag)
    {
        move_to_front(flag);
    }
    else
    {
        hop_command *temp=malloc(sizeof(hop_command));
        if(temp==NULL)
        {
            return;
        }
        strncpy(temp->path,path,PATH_SIZE-1);
        temp->hits=1;
        temp->next=head;
        temp->path[PATH_SIZE-1]='\0';
        head=temp;
        if(tail==NULL)
        {
            tail=temp;
        }
    }
}

//get most nuner of hits for having substrng key in path
hop_command* find_match_at_index(const char* key)
{
    hop_command *match = NULL;
    hop_command *temp = head;

    while(temp != NULL)
    {
        if(strstr(temp->path, key) != NULL)
        {
            if(match == NULL || temp->hits > match->hits)
            {
                match = temp;
            }
        }

        temp = temp->next;
    }

    return match;
}
///ai generated and modified waale parts:

static int path_is_dir(const char *path)
{
    struct stat st;
    return stat(path,&st)==0 && S_ISDIR(st.st_mode);
}

int hop_one(const char *arg, const char *home_dir)
{
    char before[PATH_SIZE];
    if(getcwd(before,sizeof(before))==NULL) before[0]='\0';

    const char *target=NULL;

    if(strcmp(arg,"~")==0){
        target=home_dir;
    }
    else if(strcmp(arg,".")==0){
        return 0; /* explicit no-op */
    }
    else if(strcmp(arg,"..")==0){
        if(strcmp(before,"/")==0) return 0; /* CWD has no parent */
        target="..";
    }
    else if(strcmp(arg,"-")==0){
        if(prev_cwd[0]=='\0') return 0; /* no previous CWD yet */
        target=prev_cwd;
    }
    else {
        /* requirement 5: direct relative/absolute resolution first */
        if(path_is_dir(arg)){
            target=arg;
        }
        else {
            /* requirement 6+4: frecency fallback, pruning stale hits */
            hop_command *match;
            while((match=find_match_at_index(arg))!=NULL){
                if(path_is_dir(match->path)){
                    target=match->path;
                    break;
                }
                del_node(match);
                save_cache();
            }

            if(target==NULL){
                printf("hop: no such directory\n");
                return 1;
            }
        }
    }

    if(chdir(target)!=0){
        printf("hop: no such directory\n");
        return 1;
    }

    strncpy(prev_cwd, before, PATH_SIZE-1);
    prev_cwd[PATH_SIZE-1]='\0';

    char after[PATH_SIZE];
    if(getcwd(after,sizeof(after))!=NULL)
        {
            update_cache(after);
            save_cache();
        }
    return 0;
}
int hop(int argc, char **argv, const char *home_dir)
{
    load_cache();
    if(argc==0)
        return hop_one("~", home_dir);

    int status=0;
    for(int i=0; i<argc; i++){
        if(hop_one(argv[i], home_dir)!=0)
            status=1;
    }

    return status;
}

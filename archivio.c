#include "xerrori.h"
#include <search.h>
#define QUI __LINE__, __FILE__
#define Num_elem 1000000
#define PC_buffer_len 10


// -------------------------------funzione per la protezione della hash table--------------------------------------------------------------------------------------------------------
typedef struct
{
  int readers;           // i tred che stanno leggendo
  bool writing;          // se si sta scrivendo
  pthread_cond_t cond;   // condition variable
  pthread_mutex_t mutex; // mutex associato alla condition variable
} rw;
   
// inizializza rw, ne scrittori ne lettori
void rw_init(rw *z)
{
  z->readers = 0;
  z->writing = false;
  xpthread_cond_init(&z->cond, NULL, QUI);
  xpthread_mutex_init(&z->mutex, NULL, QUI);
}

// inizio uso da parte di un reader
void read_lock(rw *z)
{
  pthread_mutex_lock(&z->mutex);
  while (z->writing == true)                // se c'è qualcuno in scrittura metto in attesa
    pthread_cond_wait(&z->cond, &z->mutex); // attende fine scrittura
  z->readers++;
  pthread_mutex_unlock(&z->mutex);
}

// fine uso da parte di un reader
void read_unlock(rw *z)
{
  assert(z->readers > 0); // ci deve essere almeno un reader (me stesso)
  assert(!z->writing);    // non ci devono essere writer
  pthread_mutex_lock(&z->mutex);
  z->readers--; // cambio di stato
  if (z->readers == 0)
    pthread_cond_signal(&z->cond); // da segnalare ad un solo writer uno alla volta perchè dveono scrivere
  pthread_mutex_unlock(&z->mutex);
}

// inizio uso da parte di writer
void write_lock(rw *z)
{
  pthread_mutex_lock(&z->mutex);
  while (z->writing || z->readers > 0) // mi fermo solo quando qualcunaltro sta scrivendo o qualcuno sta leggendo
    // attende fine scrittura o lettura
    pthread_cond_wait(&z->cond, &z->mutex);
  z->writing = true;
  pthread_mutex_unlock(&z->mutex);
}

// fine uso da parte di un writer
void write_unlock(rw *z)
{
  assert(z->writing);
  pthread_mutex_lock(&z->mutex);
  z->writing = false; // cambio stato
  // segnala a tutti quelli in attesa
  pthread_cond_broadcast(&z->cond);
  pthread_mutex_unlock(&z->mutex);
}
//-----------------------------------------------------------------------------------------------------------------------------

// --------------------------------------Funzioni gestione tabella hash -------------------------------------------------------------------------------------------------

// Struct per la gestione del valore e per il puntatore alla lista (contente tutte le Entry utile per la deallocazione)
typedef struct
{
  int *valore;
  ENTRY *next;
} coppia;

//Distrugge una ENTRY
void distruggi_entry(ENTRY *e)
{
  free(e->key);
  free(((coppia*) e->data)->valore);
  free(e->data);
  free(e);
}
//Se invocata distrugge tutta la tabella hash (funzione ricorsiva)
void termina_tabella(ENTRY *testa){
  if (testa==NULL) return;
  coppia *c = (coppia *)testa->data;
  termina_tabella(c->next);
  free(testa->key);
  free(c->valore);
  free(c);
  free(testa);
}

//crea la entry: alloca sia la stringa (la chiave del valore nella tabella hash), alloca data e lo alloca come coppia dove al suo interno è persente il valore inizializato 1 e il puntatore alla lista che per ora è inizializzato a NULL  
ENTRY *crea_entry(char *s)
{
  ENTRY *e = malloc(sizeof(ENTRY));
  if (e == NULL){
    termina("errore malloc entry 1");
  }
  e->key = strdup(s); // salva copia di s
  if (e->key == NULL ){
    free(e);
    termina("errore malloc entry 2");
  }
  
  e->data = (coppia *)malloc(sizeof(coppia));
  
  if (e->data == NULL){
    free(e->key);
    free(e);
    termina("errore malloc entry 2");
  }
  
  ((coppia*) e->data)->valore=(int*) malloc(sizeof(int));
  if (((coppia*) e->data)->valore == NULL){
    free(e->key);
    free(e->data);
    free(e);
    termina("errore allocazione");
  }
  *((coppia *) e->data)->valore  = 1;
  
  ((coppia *) e->data)->next = NULL;

  return e;
}
// aggiungi: funzione che aggiunge nuovi elemnti alla hash table se esistono se no aumenta il contattore della entry corrente
void aggiungi(char *s, ENTRY **testa)
{
  ENTRY *ris = crea_entry(s); 
  ENTRY *sup;
  sup = hsearch(*ris, FIND); 
  if (sup == NULL) 
  {                 
    sup = hsearch(*ris, ENTER); 
    if (sup == NULL) {
      distruggi_entry(ris);
      termina("problema hsearch");
    }
    coppia *c = (coppia *)ris->data;
    c->next = *testa;
    *testa = ris;
  }
  else 
  {
    *((coppia*)sup->data)->valore +=1;
    distruggi_entry(ris); 
  }
}
// conta funzione che restituisce il nuomero di occorenze nella hash tabele
int conta(char *s)
{
  ENTRY *ris = crea_entry(s);
  ENTRY *sup;
  sup = hsearch(*(ris), FIND);
  distruggi_entry(ris);
  if (sup != NULL)
  {
    int* c = (int*)((coppia*) sup->data)->valore;
    return *c;
  }

  return 0;
}
//---------------------------------------------------------------------------------------------------------------------------------------------------

//-------------------------------------------Tutte le struct--------------------------------------------------------------------------
typedef struct
{
  char **buffer;
  int *cindex;
  sem_t *sem_free_slots;
  sem_t *sem_data_items;
  pthread_mutex_t *pmutex_buf;
  rw *secure;
  int *aggiungi;
  ENTRY **testa_lista_entry;
} dati_S;

typedef struct
{
  FILE *file;
  pthread_mutex_t *muLfile;
  char **buffer;
  int *cindex;
  sem_t *sem_free_slots;
  sem_t *sem_data_items;
  pthread_mutex_t *pmutex_buf;
  rw *secure;
} dati_L;

typedef struct
{
  pthread_t *lettori;
  int Nl;
  char **buffer;
  int *pcindex;
  sem_t *sem_free_slots;
  sem_t *sem_data_items;
} dati_Capo_L;

typedef struct
{
  pthread_t *scrittori;
  int Ns;
  char **buffer;
  int *pcindex;
  sem_t *sem_free_slots;
  sem_t *sem_data_items;

} dati_Capo_S;

typedef struct
{
  pthread_t *capo_L;
  pthread_t *capo_S;
  pthread_cond_t *cond;  
  pthread_mutex_t *mutex;
  int *N_aggiungi;
  ENTRY **testa_lista_entry;
  rw *secure;
} dati_gest_segnali;

//-------------------------------------------------------------------------------------------------------------------------------------------

//---------------------------------------------- capo lettore - lettore -----------------------------------------------------------------------

void *funz_lett(void *arg)
{
  dati_L *a = (dati_L *)arg;
  char str[] = "-1";
  char *c;
  while (true)
  {
    xsem_wait(a->sem_data_items, QUI);
    xpthread_mutex_lock(a->pmutex_buf, QUI);
    c = a->buffer[*(a->cindex) % PC_buffer_len];
    *(a->cindex) += 1;
    xpthread_mutex_unlock(a->pmutex_buf, QUI);
    xsem_post(a->sem_free_slots, QUI);

    if (strcmp(c, str) == 0) // per il seganle di terminazione
    {
      free(c);
      break;
    }
  
    read_lock(a->secure); // lock per le letture della tabella di hash infatti c'è conta(c)
    xpthread_mutex_lock(a->muLfile, QUI); // lock per il file
    fprintf(a->file, "%s %d\n", c, conta(c));
    xpthread_mutex_unlock(a->muLfile, QUI);
    read_unlock(a->secure);
    free(c);
  }
  return NULL;
}

void *funz_C_lett(void *arg) 
{
  dati_Capo_L *a = (dati_Capo_L *)arg;
  char delimitatore[] = ".,:; \n\r\t";
  char *stato = NULL;

  int fd = open("capolet", O_RDONLY);
  if (fd<0) {
    
    xtermina( "Errore appertura pipe capo lettore",QUI);
  }
  while (true)
  {
    int dataLunghezza; //lunghezza stringa 
    ssize_t e = read(fd, &dataLunghezza, sizeof(int));
    int dataLength = ntohl(dataLunghezza); //converto da byte(big-endian) a interi
    
    if (e == 0){
      break; 
    } 

    char *data = (char *)malloc(dataLength);
    if (data == NULL){
      xtermina("errore nella allocazione di data",QUI);
    }
    ssize_t g = read(fd, data, dataLength);
    if (g == 0)
    {
      free(data);
      break; // quando non riesco più a leggere esco da questo while
    }
    
    char *NewSQ = (char *)malloc(dataLength + 1);
    if (NewSQ == NULL){
      xtermina("errore nella allocazione di NewSQ",QUI);
    }
    
    memcpy(NewSQ, data, dataLength); // copio la stringa in NewSQ
    NewSQ[dataLength] = 0x00;
    char *token = strtok_r(NewSQ, delimitatore, &stato);
  
    while (token != NULL)
    {
      char *duplicate = strdup(token);
      
      if (duplicate == NULL)
      {
        free(data);
        free(NewSQ);
        xclose(fd, QUI);
        xtermina("Errore durante la duplicazione della stringa", QUI);
      }
      xsem_wait(a->sem_free_slots, QUI);
      a->buffer[*(a->pcindex) % PC_buffer_len] = duplicate;
      *(a->pcindex) += 1;
      xsem_post(a->sem_data_items, QUI);

      token = strtok_r(NULL, delimitatore, &stato); //aggiorno il token
 
    }
    // elinimino queste stringhe perchè sono inutili perchè sono già state tokenizzate 
    free(data);
    free(NewSQ);
  }
  xclose(fd, QUI);

  // Segnale di terminazione
  for (int i = 0; i < a->Nl; i++)
  {
    char *stringa = strdup("-1");
    if (stringa == NULL) xtermina("Errore durante la duplicazione della stringa", QUI);
    xsem_wait(a->sem_free_slots, QUI);
    a->buffer[*(a->pcindex) % PC_buffer_len] = stringa;
    *(a->pcindex) += 1;
    xsem_post(a->sem_data_items, QUI);
  }
  //aspetto i lettori
  for ( int j = 0; j<a->Nl; j++){
    xpthread_join(a->lettori[j], NULL, __LINE__, __FILE__);
  }

  return NULL;
}
//----------------------------------------- scrittori e capo scrittori -----------------------------------------------------------------------
void *funz_scrit(void *arg)
{
  char str[] = "-1";
  dati_S *a = (dati_S *)arg;
  char *c;
  
  while (true)
  {
    xsem_wait(a->sem_data_items, QUI);
    xpthread_mutex_lock(a->pmutex_buf, QUI); // queste lock sono fatte perchè abbiamo più thread che accedono al buffer
    c = a->buffer[*(a->cindex) % PC_buffer_len];
    *(a->cindex) += 1;
    xpthread_mutex_unlock(a->pmutex_buf, QUI);
    xsem_post(a->sem_free_slots, QUI);
    
    if (strcmp(c, str) == 0)
    {
      free(c);
      break;
    }

    write_lock(a->secure); // locco lo scrittore
    aggiungi(c,(a->testa_lista_entry));// aggiungo
    write_unlock(a->secure); // lascio la lock

    read_lock(a->secure);
    if (conta(c) == 1)
    {
      *(a->aggiungi) += 1; // aggiorno il contattore delle strighe lette una volta
    }
    read_unlock(a->secure);

    free(c); // la striga c non mi serve più perchè è stata allocata in aggiungi
  }
  
  return NULL;
}

void *funz_C_scri(void *arg)
{
  dati_Capo_S *a = (dati_Capo_S *)arg;
  char delimitatore[] = ".,:; \n\r\t";
  char *stato = NULL;

  int fd = open("caposc", O_RDONLY);
  if (fd <0){ 
    termina("errore appertura pipe capo scrittore");
  }
  while (true)
  {
    int dataLunghezza;
    ssize_t e = read(fd, &dataLunghezza, sizeof(int));
    int dataLength = ntohl(dataLunghezza); ///converto da byte(big-endian) a interi
    
    if (e == 0){
      break; // se non riesce a leggere niente esce
    }

    char *data = (char *)malloc(dataLength);
    if (data == NULL){
      xtermina("errore nella allocazione di data",QUI);
    }
    
    ssize_t g = read(fd, data, dataLength);
    if (g == 0)
    {
      free(data);
      break;
    }

    char *NewSQ = (char *)malloc(dataLength + 1);
    if (NewSQ == NULL){
      xtermina("errore nella allocazione di NewSQ",QUI);
    }
    // Copiare i dati esistenti
    memcpy(NewSQ, data, dataLength);

    // Aggiungere un byte uguale a 0 alla fine
    NewSQ[dataLength] = 0x00;
    
    char *token = strtok_r(NewSQ, delimitatore, &stato);

    // metto i dati tokenizzati nel buffer
    while (token != NULL)
    {
      char *duplicate = strdup(token); 
      
      if (duplicate == NULL)
      {
        free(NewSQ);
        free(data);
        xclose(fd,QUI);
        xtermina("Errore durante la duplicazione della stringa", QUI);
      }
      // metto i dati nel buffer
      xsem_wait(a->sem_free_slots, QUI);
      a->buffer[*(a->pcindex) % PC_buffer_len] = duplicate;
      *(a->pcindex) += 1;
      xsem_post(a->sem_data_items, QUI);
      token = strtok_r(NULL, delimitatore, &stato);
      
    }
    free(data);
    free(NewSQ);
  }
  xclose(fd, QUI);
  // segnale di terminazione
  for (int i = 0; i < a->Ns; i++)
  {
    char *stringa = strdup("-1");
    xsem_wait(a->sem_free_slots, QUI);
    a->buffer[*(a->pcindex) % PC_buffer_len] = stringa;
    *(a->pcindex) += 1;
    xsem_post(a->sem_data_items, QUI);
  }
  // aspetto gli scrittori
  for ( int j = 0; j<a->Ns; j++){
    xpthread_join(a->scrittori[j], NULL, __LINE__, __FILE__);
  }

  return NULL;
}
 
//-------------------------------------------gestione segnali-----------------------------------------------------------------------------------
void *funz_gest_segnali(void *arg)
{
  dati_gest_segnali *a = (dati_gest_segnali *)arg;
  sigset_t mask;  //maschera 
  sigfillset(&mask); // ci inserisco tutti i segnali 
  int s;
  
  while (true)
  {
    char buffer[50];
    int e = sigwait(&mask, &s); // aspetto i segnali nella maschera e salvo il N del seganle in s
    if (e != 0)
    {
      xtermina("errore sigwait", QUI);
    }
    if (s == 2)
    {
      char *messaggio0 = "Numero strighe distinte inserite:";
      sprintf(buffer, "%s %d\n", messaggio0, *(a->N_aggiungi));
      ssize_t scritti = write(2, buffer, strlen(buffer)); // uso la write perchè deve essere async-signal-safe
      if (scritti == -1){
        xtermina("Errore nell'apertura del file",QUI);
      }
    }
    else if (s == 15)
    {
      //devo terminare il programma quindi aspetto i capi che finiscano il lavoro
      xpthread_join(*a->capo_S, NULL, __LINE__, __FILE__);
      xpthread_join(*a->capo_L, NULL, __LINE__, __FILE__);
    
      char *messaggio1 = "Numero totale di strighe distinte inserite:";
      sprintf(buffer, "%s %d\n", messaggio1, *(a->N_aggiungi));
      ssize_t scritti = write(1, buffer, strlen(buffer));
      if (scritti == -1){
        xtermina("Errore nell'apertura del file",QUI);
      }
      termina_tabella(*(a->testa_lista_entry)); //deallocco la tebella con la funzione 
      hdestroy();
      break;
      
    }
    else if (s == 10)
    {
      write_lock(a->secure);
      termina_tabella(*(a->testa_lista_entry));
      *(a->testa_lista_entry) = NULL;
      hdestroy();
      *(a->N_aggiungi) = 0;
      hcreate(Num_elem);
      write_unlock(a->secure);
    }

  }
  pthread_exit(NULL);
}

//-----------------------------------------------------------------------------

int main(int argc, char const *argv[])
{
  // segnali:
  sigset_t mask;
  sigfillset(&mask);
  pthread_sigmask(SIG_BLOCK, &mask, NULL); // blocco tutti i segnali nella maschera

  //testa lista per dealloccale la hash
  ENTRY *testa_lista_entry = NULL;

  if (argc < 3)
  {
    printf("Uso: %s *numero T scrittori* *numero T lettori*\n", argv[0]);
    exit(1);
  }
  
  int N_aggiungi = 0; // var per il numero di elemnti nella hash

  int W = atoi(argv[1]); // scrittori che fanno aggiungi
  int R = atoi(argv[2]); // lettori che fanno conta

  hcreate(Num_elem); // creo la tabella hash
  rw secure;         // creo la struttura dati per la muta esclusione per la hash
 
  rw_init(&secure);  // inzializzo la struttura dati

  

  pthread_t scrittori[W];
  pthread_t lettori[R];
  pthread_t capo_S;
  pthread_t capo_L;
  pthread_t gest_segnali;

  dati_S S[W];
  dati_L L[R];
  dati_Capo_L CL;
  dati_Capo_S CS;
  dati_gest_segnali gs;


  // thread del capo lettore-----------------------------------------------------------------
  sem_t sem_free_slots_Cl, sem_data_items_Cl;
  xsem_init(&sem_free_slots_Cl, 0, PC_buffer_len, QUI);
  xsem_init(&sem_data_items_Cl, 0, 0, QUI);
  char *bufferL[PC_buffer_len];
  int pindexCL = 0;
  CL.buffer = bufferL;
  CL.pcindex = &pindexCL;
  CL.sem_data_items = &sem_data_items_Cl;
  CL.sem_free_slots = &sem_free_slots_Cl;
  CL.lettori = lettori;
  CL.Nl = R;
  xpthread_create(&capo_L, NULL, funz_C_lett, &CL, __LINE__, __FILE__);

  // thread del capo scrittore---------------------------------------------------------------
  sem_t sem_free_slots_Cs, sem_data_items_Cs;
  xsem_init(&sem_free_slots_Cs, 0, PC_buffer_len, __LINE__, __FILE__);
  xsem_init(&sem_data_items_Cs, 0, 0, __LINE__, __FILE__);
  char *bufferS[PC_buffer_len];
  int pindexCS = 0;
  CS.buffer = bufferS;
  CS.pcindex = &pindexCS;
  CS.sem_data_items = &sem_data_items_Cs;
  CS.sem_free_slots = &sem_free_slots_Cs;
  CS.scrittori = scrittori;
  CS.Ns = W;

  xpthread_create(&capo_S, NULL, funz_C_scri, &CS, __LINE__, __FILE__);
  // thread lettori---------------------------------------------------------------------------
  FILE *file = fopen("lettori.log", "w");
  int cindexL = 0;
  pthread_mutex_t muLfile = PTHREAD_MUTEX_INITIALIZER;
  pthread_mutex_t muLbuf = PTHREAD_MUTEX_INITIALIZER;
  for (int i = 0; i < R; i++)
  {
    L[i].file = file;
    L[i].muLfile = &muLfile;
    L[i].buffer = bufferL;
    L[i].cindex = &cindexL;
    L[i].pmutex_buf = &muLbuf;
    L[i].sem_data_items = &sem_data_items_Cl;
    L[i].sem_free_slots = &sem_free_slots_Cl;
    L[i].secure = &secure;
    xpthread_create(&lettori[i], NULL, funz_lett, L + i, __LINE__, __FILE__);
  }

  // thread scrittori------------------------------------------------------------------------------

  pthread_mutex_t muSbuf = PTHREAD_MUTEX_INITIALIZER;
  int cindexS = 0;
  for (int i = 0; i < W; i++)
  {
    S[i].buffer = bufferS;
    S[i].cindex = &cindexS;
    S[i].pmutex_buf = &muSbuf;
    S[i].sem_data_items = &sem_data_items_Cs;
    S[i].sem_free_slots = &sem_free_slots_Cs;
    S[i].secure = &secure;
    S[i].aggiungi = &N_aggiungi;
    S[i].testa_lista_entry = &testa_lista_entry;
    xpthread_create(&scrittori[i], NULL, funz_scrit, S + i, __LINE__, __FILE__);
  }
  // thread gestione segnali-------------------------------------------------
  gs.N_aggiungi = &N_aggiungi;
  gs.testa_lista_entry = &testa_lista_entry;
  gs.secure = &secure;
  gs.capo_S = &capo_S;
  gs.capo_L = &capo_L;
  xpthread_create(&gest_segnali, NULL, funz_gest_segnali, &gs, __LINE__, __FILE__);

  // fine del programma----------------------------
  xpthread_join(gest_segnali, NULL, __LINE__, __FILE__); // aspetto che finsica il gestore dei segnali che è l'ultiama cosa che termina 
    //dealloco le ultime cose 
  fclose(file); //chiudo il file"lettori.log"
  xpthread_mutex_destroy(&(secure.mutex),QUI);
  xpthread_cond_destroy(&(secure.cond),QUI);
  xsem_destroy(&sem_free_slots_Cl,QUI);
  xsem_destroy(&sem_data_items_Cl,QUI);
  xsem_destroy(&sem_free_slots_Cs,QUI);
  xsem_destroy(&sem_data_items_Cs,QUI); 
  xpthread_mutex_destroy(&muLbuf,QUI);
  xpthread_mutex_destroy(&muSbuf,QUI);
  return 0;
}

# DESCRIZIONE PROGETTO

Il seguente codice implementa un programma concorrente con un'architettura di rete **server client**, utilizando **archivio** che gestisce una tabella hash in cui vengono inserite stringhe da parte di scrittori e lette da parte di lettori. Il programma utilizza la mutua esclusione per garantire la coerenza dei dati nella tabella hash.

## ARCHIVIO.C

Il programma è un'applicazione concorrente scritta in linguaggio ***C*** che gestisce una tabella hash attraverso operazioni di lettura e scrittura concorrenti. Il programma è progettato per essere eseguito da riga di comando, con il numero di scrittori e lettori specificati come argomenti. Durante l'esecuzione, il programma gestisce segnali per consentire un controllo flessibile delle operazioni in corso.

### Mutua Esclusione per la Hash Table

Il codice inizia definendo una struttura (`rw`) per la gestione della mutua esclusione nella tabella hash. Questa struttura contiene contatori per i lettori, una variabbile per indicare se uno scrittore sta scrivendo, una variabile di condizione (`cond`) e un mutex associato (`mutex`).

Le funzioni seguenti vengono definite per inizializzare la struttura (`rw_init`), acquisire un lock di lettura (`read_lock`), rilasciare un lock di lettura (`read_unlock`), acquisire un lock di scrittura (`write_lock`), e rilasciare un lock di scrittura (`write_unlock`). Questa implemntazione favorisce i lettori come da consegna.

### Funzioni per la Gestione della Tabella Hash

Segue la definizione di funzioni per la gestione della tabella hash. In particolare, le funzioni includono:

+ `distruggi_entry`: Libera la memoria associata a una singola entry.
+ `termina_tabella`: Libera la memoria associata a tutta la tabella hash in modo ricorsivo, attraverso un puntattore (salvato nel main) a una lista con tutte le occorrenze della tabella.
+ `crea_entry`: è responsabile della creazione di un nuovo elemento ENTRY per la tabella hash. La funzione accetta una stringa (*s*) come parametro, che sarà utilizzata come chiave per l'elemento della tabella hash. viene allocata memoria per un nuovo oggetto ENTRY, per la stringa s, per un oggetto di tipo coppia (definito nella struttura ENTRY), che contiene il valore inizializzato a 1 (perchè la stringa è nuova quindi esiste solo un occorenza) e un puntatore alla lista delle entry (inizializzato a NULL) per tenere traccia delle entry in modo da deallocarla successivamente.
+ `aggiungi`: La funzione aggiungi è progettata per gestire l'aggiunta di nuovi elementi alla tabella hash o l'incremento del contatore se la chiave esiste già. La funzione accetta una stringa (s) come chiave e un puntatore a un puntatore a ENTRY (testa), che rappresenta la testa di una lista associata a quella chiave specifica nella tabella hash. **funzionamento**: Viene creata una nuova entry utilizzando la funzione crea_entry, passando la stringa s come chiave.
Viene eseguita una ricerca nella tabella hash utilizzando la funzione hsearch con l'opzione FIND per verificare se la chiave esiste già.
Se la chiave non è presente, la nuova entry viene inserita nella tabella hash con l'opzione ENTER.
Se la chiave è già presente, il valore associato alla chiave viene incrementato di 1.
In entrambi i casi, la memoria allocata per la nuova entry viene deallocata. 
+ `conta`: La funzione conta è progettata per restituire il numero di occorrenze di una specifica chiave nella tabella hash. La funzione accetta una stringa (s) come chiave da cercare nella tabella hash.
**funzionamento**: Viene creata una nuova entry utilizzando la funzione crea_entry, passando la stringa s come chiave.
Viene eseguita una ricerca nella tabella hash utilizzando la funzione hsearch con l'opzione FIND per verificare se la chiave esiste.
Se la chiave è presente, viene restituito il valore associato a quella chiave, che rappresenta il numero di occorrenze.
Se la chiave non è presente, viene restituito 0, indicando che non ci sono occorrenze per quella chiave.

**Note**: Queste funzioni in primo luogo conta e aggiungi accedono alla hash table, per questo motivo all'interno delle funzioni successive verranno protette da delle lock (vedi paragrafo "Mutua Esclusione per la Hash Table"). 

### Struct per la Gestione delle Operazioni su Buffer

Sono definite diverse strutture dati (`dati_S`, `dati_L`, `dati_Capo_L`, `dati_Capo_S`, `dati_gest_segnali`) per passare i dati tra i thread. Queste strutture includono variabili come array di buffer, contatori, semafori, mutex e altre informazioni necessarie per coordinare le operazioni tra i thread, tutte queste strutture sono state inizializzate nel main.

### Thread per la Lettura

**`Capo lettore`** :
La funzione `funz_C_lett` è responsabile della lettura da una pipe (`capolet`) e del tokenizzazione delle stringhe lette. Questa funzione è progettata per essere eseguita dai thread Capo Lettore, che hanno il compito di gestire la lettura da una sorgente esterna (in questo caso, una pipe con nome) e di inviare in modo uniforme le stringhe tokenizzate ai thread Lettori. **funzionamento**: Inizializzazione di una variabile delimitatore contenente i caratteri che verranno utilizzati per tokenizzare le stringhe (",.:; \n\r\t").
Apertura della pipe `capolet` in modalità di sola lettura.
Inizia un loop che legge dalla pipe e processa i dati fino a quando non è possibile leggere più dati dalla pipe.
All'interno del loop, vengono letti la *lunghezza* e i *dati* dalla pipe. La lunghezza viene convertita da formato big-endian a intero e i dati vengono rapprentati come strighe.
Viene allocata memoria per i dati letti e copiata la stringa nella nuova area di memoria con lungehezza + 1 per inserire il carettere di fine stringa.
La stringa viene tokenizzata utilizzando `strtok_r` (ho utilizzato questa funzione perché è thread-safe è in un programma di questo tipo mi sembrava ideale), e i token vengono inviati ai thread Lettori attraverso il buffer condiviso protteto opportunamente da dei semafori.
Il processo continua finché non si raggiunge la fine della pipe o si verifica un errore di lettura.
Al termine della lettura dalla pipe, viene inviato un segnale di terminazione ai thread Lettori, inviando una stringa speciale ("-1") per ciascun thread Lettore.
Inoltre il capo aspetta che i suoi lettori finiscano in modo da tenere il tutto sincronizzato. in fineIl thread Capo Lettore termina.

**`Lettori`**: i lettori sono chiamati dal main e implementati dall funzione `funz_lett` è progettata per essere eseguita come thread e gestisce il compito di leggere da un buffer condiviso con il Capo lettore e aggiornare il file `lettori.log` di output con le informazioni ottenute dalla funzione `conta` applicata alla tabella hash infatti questa operazione viene protteta dalla mutua esclusione sia per il file che per la tabella hash. Questo processo avviene all'interno di un ciclo while che continua finché non viene letto un segnale di terminazione ossia (-1) dal buffer.
### Thread per la Scrittura
**`Capo scrittore`**: Il capo scrittore è impelmentato dalla funzione funzione `funz_C_scri` è progettata per essere eseguita come singolo thread e gestisce il compito di leggere da una pipe con nome(canale di comunicazione tra processi) `"caposc"` e aggiornare un buffer condiviso con gli scrittori. Questo processo avviene all'interno di un ciclo while che continua finché la pipe non è vuota, infine aspetta i thread scrittori, indicando la fine del processo. l'implemntazione di questo thread è molto simile al capo lettore, se non identica l'unica differenza sta nel fatto che si legge dalla pipe caposc e si inviano le strighe ai thread scrittori.

**`Scrittori`**: La funzione `funz_scrit` è progettata per essere eseguita concorrentemente da più thread scrittori. Aggiorna una tabella hash e un contatore condiviso tra i thread e il main e il thread per la gestione dei segnali si arresta quando riceve il segnale di terminazione (*-1*). L'esecuzione concorrente è coordinata utilizzando un lock di mutua esclusione e semafori per garantire la correttezza e la consistenza dei dati condivisi. Riceve le strighe tokenizzate dal buffer protteto da un semaforo e da un mux aggiorna la tabella hash aggiungendo queste strighe l'operazione è poteta dalla mutua esclusione, successivamente incrementa il contattore se la striga è nuova e questa cosa viene verificata dalla funzione `conta` sempre protta dalla mutua esclusione.

### Gestione dei Segnali

Viene definito un thread (`funz_gest_segnali`) per gestire i segnali. Questo thread utilizza `sigwait` per attendere segnali specifici e risponde di conseguenza, ed è l'unico thread che può ricevere i segnali perchè sono stati bloccati tutti i sganli dal main. In particolare, i segnali 2 (SIGINT), 15 (SIGTERM), e 10 (SIGUSR1) vengono gestiti.
**funzionamento**: setto una maschera dove inserisco tutti i segnali, aspetto il seganle con la `sigwait` e salvo il numero nella variabbile `n` con una serie di if decido cosa fare il tutto è al interno del loop che si ferma solamente quando riceve il segnale `15`. a seconda del segnale che riceve il thread si fanno azioni differenti:

+ Con il segnale numero `2` stampo in `stderr` il numero di scritte fino a al momento in cui è chiamato il segnale. la stampa viene fatta `async-signal-safe` infatti uso la write, mi creo un buffer statico in cui inserisco la stringa e il numero delle srighe inserite attraverso la funzione `sprintf` che scrive sul buffer.
+ Con il segnale nuemro `15` invece si aspetta che termini il tutto faccendo la join sul capo lettore e scrittore, successivamente stampa il numero di strighe(attraverso una variabbile che è condivisa con gli scrittori) all'intero della hash tabel su stdout sempre in modo `async-signal-safe` come ho fatto per il seganle 2; poi si dealloca tutta la hash tabel con la funzione `termina_tabella` che dealloca tutta la memoria all'interno e successivamente distruggo la hash tabele e si fa un brack che esce dal ciclo e termina il thread con la terminzione di quest'ultimo si chiude l'intero programma perchè nel main c'è la join si questo thread e successivamente la liberazione di tutta la memoria, quindi chiamare questo segnale è l'unico modo per terminare il programma.
+ Con il segnale 10 invece c'è si applica una specie di reset alla tabella infatti si prende la lock per la scrittura, dealloca tutta la tabella hash, si termina la tabella, si azzera la variabbile che rappresnta il numero di strighe inserite, e si crea una nuova tabella.

### Il main

il cervello del programma è il main. Inizializza variabili, crea tutti i thread e gestisce il flusso principale del programma, inzializza tutte le strutture. Gestisce anche la chiusura del programma, assicurandosi che tutte le risorse vengano deallocate correttamente.


## IL SERVER
Questo file descrive un server scritto in Python che gestisce connessioni da client di due tipi distinti, "Tipo A" e "Tipo B". Il server utilizza socket per ascoltare su un indirizzo IP e una porta specifici.

### Funzione `main`

La funzione principale `main` avvia il server:
- Crea le pipe (`caposc` e `capolet`) e le apre in scrittura.
- Avvia il processo dell'archivio con eventuali opzioni Valgrind.
- Utilizza un pool di thread per gestire le connessioni dei client, che si connttono attraverso una soket e le loro connessioni vengono gestite signolaremnte da ogni thread che utilizza la funzione `handle_client` per gestire i tipi di connessione.
- Termina il server in modo pulito alla ricezione di un'interruzione da tastiera (`KeyboardInterrupt`). quando avvine questo comando vene chiusa la soket il lettura e scrittura si manda il segnale SIGTERM ad archivio in modo da far terminare il processo in modo pulito, si eliminano e cancellano le pipe, e in fine si aspetta `archivio` perchè vogio che termini prima archivio che il server.

### Funzione `handle_client`
Questa funzione gestiscce la connessione del server è un thread, Il server riceve un numero intero (4 byte) che indica il tipo di connessione (`Tipo A` o `Tipo B`) 0 di tipo A, 1 di tipo B.
Il server interpreta il tipo di connessione e lo registra nel file di log. In base al tipo di connessione ci si indirizza nei rami del if. 

- Se il tipo di connessione è **"Tipo A"**, il server riceve dalla soket la lunghezza della sequenza (4 byte) la spachetta in un intero e quindi la sequenza di byte.
Scrive la lunghezza e la sequenza di byte nelle pipe (capolet) per il processo di archivio.
Aggiorna il numero totale di byte gestiti.

- Se il tipo di connessione è **"Tipo B"**, il server continua a ricevere blocchi di byte fino a quando non riceve una sequenza di lunghezza 0.
Scrive le sequenze di byte nelle pipe (caposec) per il processo di archivio.
Invia al client la somma totale delle lunghezze delle sequenze gestite.

Tutte le operazioni di scrittura sono prottete dalla lock questo perchè è vero che i thread in Python non procedono parallelamente ma sequenzialemnte ma potrebbe causare dei problemi lo stesso questo perchè potrebbero essere deschedulati in modo non consono, ad esempio se il thrad è entratto nella lock e si ferma per qualche motivo e viene eseguito un altro thread che fa la stessa cosa potrebbe causare problemi nella scrittura se non ci fosse la lock.

### Funzione `recv_all`

La funzione `recv_all` riceve un numero specifico di byte dalla connessione del client utilizzando blocchi di 1024 byte.

### Argomenti da Linea di Comando e loging
Gli argomenti da linea di comando includono il numero di thread, il numero di lettori e scrittori, l'indirizzo IP e la porta. Il modulo `argparse` facilita la configurazione del server tramite la riga di comando.
inoltre per capire che succede all'intero del programma si usa un file di `log`.


## CLIENT 1

Il client 1 è un file in C, `client1.c` stabilisce connessioni di tipo A con il server; inviando una stringa di un file una dopo l'altra.
+ Infatti manda un intero (4 byte) in formato network corrispondente a 0 
+ Successivamente manda la lunghezza della linea del file (uso la getline) sempre in formato network la invio nella soket, e successivamente mando i byete della stringa sempre nella soket.
+ Chiudo la soket

Questo prgramma è stato progettatto in modo tale da fare diverse richieste di connessione al server per la perecisione per ogni liniea del file avviene una connessione al server. per inviare e ricevere byte dal server utilizzo due funzionini `writen` e `readn` sono comunemente utilizzate per leggere e scrivere dati su un descrittore di file (file descriptor), come una socket in questo caso, in modo che l'operazione sia completa e affidabile rispetto a chiamate di sistema read e write standard. Entrambe le funzioni utilizzano un ciclo per gestire casi in cui la lettura o la scrittura non riescono completamente alla prima chiamata.

## CLIENT 2

Questo client è un programma in Python `client2.py`, è un programma multi-thread infatti associa a ogni thread un file che dovrà leggere e inviare ogni riga del file al soket, in questa implemntazione viene stabilitta un unica connessione per file. 
+ Il main crea un pool di thread, a ogni thread viene passato unico file, infatti il numero di thread è sterattamente collegato al numero di file passati da riga di comando. la funzione che gestisce questi thread si chiama `client2`.
+ client2 si occupa della connessione alla soket, apre il file e inzia a invieare le righe del file inviando prima la lunghezza inpachettata e successivamente manda la sequenza di byte al server, per far capire al server che ha finito alla fine manda una lunghezza pari a 0, e termina.
Per gli argomenti in questo caso viene utilizzata la libreria sys. 
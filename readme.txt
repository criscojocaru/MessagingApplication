===============================================================================
Nume: Cojocaru Cristina-Gabriela
Grupa: 321CD
===============================================================================

== TEMA 2 PC ==

Exemplu de rulare Makefile:
make all
make run_server PORT=1500
make run_subscriber ID=cris IP_SERVER=127.0.0.1 PORT=1500

Am realizat o aplicatie client-server TCP si UDP pentru gestionarea mesajelor,
implementata in C++ si avand ca schelet laboratorul 8 (multiplexare TCP).

Serverul deschide 2 socketi, unul UDP si unul TCP pe portul primit ca
parametru si asteapta conexiunea clientilor TCP sau datagrame de la clientii
UDP pe care le va trimite mai departe catre clientii TCP abonati. Serverul se
inchide la comanda exit citita de la tastatura.

Clientul TCP trimite catre server ID-ul atunci cand are loc conexiunea. Acesta
poate primi de la tastatura comenzile:
- exit -> deconectare client
- subscribe topic SF -> abonare client la topic si activare/dezactivare 
functionalitate store & forward
- unsubscribe topic -> dezabonare client de la topic
Clientul poate primi de la server:
- mesajele de pe topic-urile la care este abonat
- exit -> serverul s-a inchis, deci clientul se va deconecta automat

Am dezactivat algoritmul Nagle, aplicand functia setsockopt cu optiunea 
TCP_NODELAY pe socket-ul TCP. De asemenea, am folosit aceasta functie cu 
optiunea SOL_SOCKET pe socket-ul TCP si cel UDP pentru a putea folosi portul
primit ca parametru, chiar daca acesta este ocupat ("ERROR on binding: Address
already in use").

Pentru reprezentarea clientilor si a topic-urilor la care sunt abonati, am ales
sa folosesc mai multe map-uri: intre socket si ID client, intre ID client si
topic, intre topic si mesajele stored.

Am adaugat socketii TCP si UDP in multimea file descriptorilor.
Daca s-a primit o datagrama de la clientul UDP, prelucrez informatia: se 
primeste un buffer de 1551 bytes, dintre care primii 50 de bytes reprezinta
numele topic-ului, byte-ul 51 reprezinta tipul de date, iar urmatorii bytes
(maxim 1500) valoarea.
- tipul INT -> byte-ul 52 contine semnul intregului, iar urmatorii 4 bytes
valoarea in modul; am construit valoarea acestuia byte cu byte, folosind
shiftari si OR pe biti, apoi am transformat-o in string.
- tipul SHORT REAL -> valoarea este formata din al 52-lea si al 53-lea byte,
pe care am construit-o la fel, folosind shiftari si OR pe biti, apoi am 
impartit-o la 100.0 si am transformat-o in string
- tipul FLOAT -> am construit valoarea intreaga la fel ca la tipul INT, apoi 
am inmultit-o cu 1.0 si am impartit la 10 la puterea byte-ului 57
- tipul STRING -> am pus byte cu byte intr-un buffer de char-uri

Dupa ce am prelucrat informatia, am trimis-o catre clientii abonati la topic-ul
respectiv. In cazul in care clientul este deconectat si are SF activat pentru
acel topic, pastrez informatia pentru a o trimite atunci cand clientul se 
conecteaza. 

Daca a venit o cerere noua de conexiune pe un socket inactiv, serverul o
accepta si se primeste ID-ul noului client. In cazul in care clientul s-a 
reconectat si avea SF activat pentru topicul curent, serverul ii trimite
mesajele pierdute in timpul in care acesta a fost inactiv.

Daca s-au primit date pe unul din socketii de client TCP, serverul le
receptioneaza: 
- clientul a trimis "exit" -> s-a deconectat
- clientul a trimis "subscribe topic SF"/"unsubscribe topic", orice alta
comanda este invalida (serverul trimite mesaj de eroare in acest caz).

OBSERVATII:
Aplicatia ruleaza cum trebuie in mare parte: la rularea mode manual / random
este totul ok. In schimb, cand rulez mode all_once, uneori clientii primesc
bine absolut toate mesajele, alteori, doar o parte din ele. Nu am gasit o 
explicatie pentru asta, credeam ca problema e TCP_NODELAY, dar dupa ce l-am 
adaugat, nu prea s-a schimbat mare lucru.







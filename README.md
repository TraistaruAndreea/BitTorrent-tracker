###################################### Traistaru Andreea-Cosmina 332CD ######################################

####### Structuri de date folosite: #######

#### Structura ClientData ########
-> retine informatiile necesare pentru un client, cum ar fi fisierele pe care le detine, fisierele pe care vrea sa le downloadeze
-> variabila all_files_downloaded: este true atunci cand clientul a descarcat toate fisierele pe care le dorea
-> variabila should_stop: este true atunci cand trackerul trimite mesaj de shutdown catre toti clientii
-> mutex pentru a proteja should_stop si all_files_downloaded

#### Structura File ########
-> retine informatiile necesare pentru un fisier, cum ar fi numele fisierului si chunck-urile acestuia
-> este o structura auxiliara pentru a fi folosita in ClientData pentru a retine fisierele pe care le detine un client

#### Structura SwarmInfo ########
-> retine swarmul fiecarui fisier, adica numele fisierului, numarul de chunckuri pe care le contine fisierul si un vector de clienti
in care disting clientii in seed si peer

#### Structura PeerData ########
-> retine rankul clientului
-> variabila booleana isPeer care este true daca clientul este peer si false daca este seed

#################################### Implementare ####################################

############# Functia tracker #############

Initial primeste informartiile de la clienti despre cate si ce fisiere are fiecare si populeaza swarmurile fiecarui fisier.
Dupa ce am primit informatiile de la toti clientii, ii anunt trimitandu-le un ack ca pot incepe descarcarile.
Apoi, in while rulam cat timp mai sunt clienti de descarcat si primim mesaje de la acestia. Daca mesajul este MSG_REQUEST_SWARM caut
file-ul respectiv si ii trimit swarmul sau.
Daca mesajul este MSG_FILE_DOWNLOADED, caut clientul care detine fisierului pentru a seta ownerul fisierului ca seed.
Daca mesajul este MSG_ALL_COMPLETED, insemana ca acel client a descarcat toate fisierele dorite si cresc contorul de clienti 
care au terminat de descarcat.

Cand am iesit din while insemana ca toti clientii au terminat de descarcat si trimit mesaj de shutdown catre toti.

############# Functia download_thread_func #############
Astept sa primesc ack de la tracker pentru a putea incepe descarcarea.Intr-un while true incep sa descarc chunckuri pana cand termin 
de descarcat toate chunckurile sau primesc mesaj de shutdown de la tracker.
Trimit trackerului numele fisierului pe care vreau sa il descarc si primesc swarmul sau. Pentru a alterna clientii am mai folosit un while
in care am cerut pe rand cate un chunck din fisier. Am alternat clientii intrebandu-i pe rand daca au chunckul respectiv si daca da, 
cresteam contorul de chunckuri descarcate si il adaugam in owned_files. Odata la 10 chunckuri cer de la tracker lista cu swarmuri updatata.
Dupa ce termina de descarcat toate chunckurile, marchez file-ul ca descarcat si trimit mesaj de MSG_FILE_DOWNLOADED la tracker.
Cand toate fisierele sunt descarcate, trimit mesaj de tipul MSG_ALL_COMPLETED catre tracker.

############# Functia upload_thread_func #############
Folosesc MPI_IProbe pentru a nu bloca while-ul in cazul in care primesc mesaj de shutdown de la tracker.Primesc de la clienti file-ul 
pe care vor sa il descarce si chunckul. Caut in lista de fisiere pe care le detin si daca am chunckul respectiv, il trimit clientului.
Trimit Ok daca am chunckul si NO daca nu am chunckul respectiv.

############# Functia Peer #############
-> In functia Peer apelez citirea din fisier si primesc mesajul de shutdown de la tracker.
-> Modific variabila client->data->should_stop pentru a anunta clientii ca trebuie sa se opreasca.

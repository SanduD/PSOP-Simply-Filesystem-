# PSOP-Simply-Filesystem-
Simple File System Project
Motivație:
	Scopul acestui proiect este de a ne ajuta sa intelegem mai bine  notiunile de la baza unui Linux FS. Linux FS suporta multiple sisteme de fisiere(ext2,ext3,ext4,NTFS,etc.). File System-ul pe care urmeaza sa il implementam este bazat pe FAT care sa suporte pana la 128 de fisiere in directorul root. Layout-ul unui File System pe disc este compus din 4 parti logice consecutive si sunt dispuse in urmatoarea ordine: Superblock, FAT, Directorul Root si Blocurile Data.
	TODO: O diagramă cu cum e imaginea pe disk a FS-ului

![FAT Layout](./FAT_layout.png "FAT Layout")

<b>Superblock</b>: Este primul bloc de pe disk si contine meta-date esentiale despre file system cum ar fi semnatura discului, numarul de blocuri pe care il contine, indexul directorului root de pe disk, indexul blocului de date, cantitatea de FAT si Data blocks.

<b>File Allocation Table</b>: FAT este situat pe unul sau mai multe blocuri și ține evidența atât a blocurilor de date care nu ocupa memorie, cât și a maparii dintre fișiere și blocurile de date in care se afla conținutul. Acest bloc urmează dupa Superblock și este reprezentat ca un vector de structuri care contine variabile de 16 biti. 

<b>Root Directory</b>: Este un bloc de date care urmeaza dupa blocurile FAT si contine o intrare pentru fiecare fisier din FS. El se reprezinta ca o structura de date in care sunt declarate numele de fisiere, dimensiunea si locatia primului bloc de date pentru acest fisier. Din moment ce numarul maxim de fisiere dintr-un director este de 128 de fisiere, root dir este un vector de 128 de structuri.

<b>Data Blocks</b>:Sunt folosite pentru continutul fisierelor. Din moment ce dimensiunea fiecarui disk block virtual este de 4096 bytes, un fisier se poate intinde pe mai mult blocuri(depinde de dimensiunea indicata de propriul offset din FDT). Daca un fisier este mai mic decat dim. unui bloc, o sa ocupe tot acel bloc, indiferent de spatiul suplimentar de pe acel bloc(acest lucru o sa provoace o fragmentare interna pe discul virtual care trebuie retinuta.).

Funcționalități:

1.	<b><i>Mount/Un-mount</b></i>: sistemul de fisiere continut pe discul virtual specificat trebuie sa fie gata pentru a fi folosit. Adica toate componentele acestuia trebuie sa fie initializate sau copiate de pe alt disc virtual cu toate informatiile necesare. Pentru a face asta, blocul API trebuie sa fie gata sa citeasca fiecare bloc in memoria noastra. Pentru fiecare componenta logica programul va aloca spatiul necesar care va oferi cantitatea  de spatiu de care vom avea nevoie cand citim blocurile. In cazul Superblock-ului, desi variabilele care contin informatiile esentiale ale discului nu ocupa o dimensiune echivalenta de memorie ca un bloc de pe discul virtual, structura va contine un extra padding, care va suplimenta spatiul extra care nu este folosit si ii va da structurii dimensiunea de 4096 de bytes. Aceeasi strategie este utilizata si pentru Root Directory.
	Structura FAT are o abordare similara pentru citirea blocurilor sale in memorie, logica fiind putin modificata, pentru a se potrivi cu dimensiunea variata FAT. Deoarece citim Superblock-ul inaintea FAT-ului, programul este capabil sa cunoasca dimensiunea blocurilor FAT. 

2.	Crearea Fisierelor/Stergerea fisierelor: Permite utilizatorului sa creeze sau sa stearga din fisiere. Pentru a crea un nou fisier, programul va cauta un spatiu liber in directorul root prin verificarea primului byte al unei posibile intrari(daca este 0 atunci vom initializa intrarea cu numele fisierului primit ca argument). Pentru stergerea unui fisier programul se va asigura ca intrarea fisierului este goala si ca tot continutul blocurilor de date detinute de fisier au fost eliberate in FAT.

3.	Operatii cu File Descriptor: FDT este important in gestionarea operatiilor de deschidere si inchidere ale fisierelor. 

4.	Citirea/Scrierea in fisier: Aceste doua functionatilati sunt cele mai complexe si vor permite citirea/scrierea unui fisier de pe disc.

<b>API:</b>
-fs_mount();
-fs_umount();
-fs_info();


Arhitectura:
- elemente componente
- module
- Structuri de date folosite


Testare/Mod de utilizare (TODO 1)
- Interactiunea cu FS se va face prin intermediul unei aplicații
- ./app <partitie> <comanda> <parametri>
- ./app /dev/sdb1 ls /

Interactiune de tip bibliotecă
- ref = myfat_init("/dev/sdb/1);"
- int fd = myfat_open("/dev/sda", "r");
- myfat_read(fd, buffer, ...);

 

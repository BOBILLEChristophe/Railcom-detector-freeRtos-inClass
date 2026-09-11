# RailCom Detector ESP32 — FreeRTOS / C++ class

Projet open source de détection RailCom® pour **ESP32**, organisé sous forme de **classe C++ réutilisable**.

Cette version est destinée à l'intégration dans d'autres projets : la réception UART, le décodage RailCom et la validation de l'adresse DCC sont encapsulés dans la classe `Railcom`.

Le programme principal peut ainsi rester très simple et utiliser directement :

```cpp
railcom.address();
```

pour connaître l'adresse de la locomotive actuellement détectée.

---

## 🇫🇷 Français

### Présentation

Le détecteur RailCom matériel transforme le retour RailCom présent pendant le cutout DCC en un signal série UART.

L'ESP32 reçoit ce signal à **250 000 bauds, 8N1**, décode les symboles RailCom **4-out-of-8**, récupère les informations `ADR1` et `ADR2` du **canal 1**, puis reconstitue l'adresse DCC de la locomotive.

La particularité de ce projet est que tout ce traitement est regroupé dans une **classe C++ `Railcom`**.

Cela permet notamment :

- de masquer la complexité du décodage RailCom au programme utilisateur ;
- de réutiliser facilement le détecteur dans un projet plus important ;
- de créer plusieurs instances indépendantes ;
- d'utiliser plusieurs détecteurs RailCom sur un même ESP32, dans la limite des UART matériels disponibles.

Cette approche est particulièrement adaptée à des projets comme les satellites autonomes, où RailCom n'est qu'une fonction parmi d'autres.

---

## Fonctionnalités

- ESP32 classique / ESP32-WROOM-32.
- Réception RailCom par UART matériel.
- Vitesse UART : **250 000 bauds**.
- Format : **8N1**.
- Réception basée sur le driver UART **ESP-IDF** et FreeRTOS.
- Réception événementielle : pas de polling permanent avec `Serial.available()`.
- Traitement des octets comme un **flux continu**.
- Décodage RailCom **4-out-of-8**.
- Décodage des datagrammes `ADR1` et `ADR2` du canal 1.
- Gestion des adresses DCC **courtes et longues**.
- Validation d'une adresse après plusieurs reconstructions identiques consécutives.
- Suppression automatique de l'adresse lorsqu'aucun retour RailCom valide n'est reçu pendant un délai défini.
- Méthode simple :

```cpp
uint16_t address();
```

- `address()` retourne :
  - l'adresse DCC validée ;
  - `0` lorsqu'aucune locomotive RailCom n'est actuellement détectée.

---

## Architecture

Le projet est séparé en trois fichiers principaux :

```text
railcom_freeRtos_inClass.ino
Railcom.h
Railcom.cpp
```

### `Railcom.h`

Déclare la classe `Railcom`, son interface publique et les données nécessaires à son fonctionnement.

### `Railcom.cpp`

Contient l'implémentation :

```text
UART ESP32
    ↓
événements FreeRTOS
    ↓
flux continu d'octets
    ↓
décodage 4-out-of-8
    ↓
ADR1 + ADR2
    ↓
adresse DCC
    ↓
validation
    ↓
Railcom::address()
```

### `railcom_freeRtos_inClass.ino`

Programme d'exemple montrant comment instancier et utiliser la classe.

Le fichier `.ino` reste volontairement très court : tout le traitement RailCom est réalisé dans la classe.

---

## Principe de réception

Le détecteur matériel fournit un signal UART classique :

```text
repos logique HIGH
      ↓
start bit
      ↓
8 bits
      ↓
stop bit
```

À 250 000 bauds :

```text
1 bit UART ≈ 4 µs
1 caractère 8N1 ≈ 40 µs
```

La classe utilise directement le périphérique UART de l'ESP32.

La réception est **événementielle** : une tâche FreeRTOS attend les événements générés par le driver UART ESP-IDF et ne consomme donc pas inutilement du temps processeur lorsqu'aucune donnée n'arrive.

---

## Pourquoi un parseur continu ?

Les événements `UART_DATA` générés par le driver ESP-IDF ne correspondent pas nécessairement aux frontières des datagrammes RailCom.

Une paire peut par exemple arriver sous cette forme :

```text
événement 1 : F0 A3 AC 99
événement 2 : 8E A3 AC F0
```

Le datagramme :

```text
99 8E
```

est alors coupé entre deux événements UART.

La classe ne considère donc pas un événement UART comme une trame RailCom. Elle analyse les octets comme un **flux continu** et reconstitue elle-même les datagrammes utiles.

---

## Décodage RailCom

Le programme utilise une table de décodage **4-out-of-8**.

Les informations actuellement exploitées sont celles du **canal 1 RailCom** :

```text
ID 1 → ADR1
ID 2 → ADR2
```

`ADR1` contient la partie haute de l'adresse et permet également de distinguer une adresse courte d'une adresse longue.

`ADR2` contient la partie basse.

Exemples validés :

```text
Adresse 12

ADR1 = 0
ADR2 = 12
→ adresse = 12
```

```text
Adresse 6400

ADR1 = 153
ADR2 = 0
→ adresse = 6400
```

Le canal 2 RailCom n'est pas exploité par cette version de la classe.

---

## Validation de l'adresse

Afin d'éviter qu'un symbole parasite ne provoque une fausse identification, une adresse n'est acceptée qu'après plusieurs reconstructions identiques consécutives.

La version actuelle utilise un filtrage temporel léger, bien plus réactif que l'ancien système basé sur un grand buffer circulaire.

Lorsqu'aucun retour valide de la locomotive n'est reçu pendant le délai prévu, l'adresse est remise à :

```cpp
0
```

La disparition d'une locomotive peut donc être détectée simplement avec :

```cpp
if (railcom.address() == 0)
{
    // aucune locomotive détectée
}
```

---

## Utilisation

Inclure la classe :

```cpp
#include "Railcom.h"
```

Créer une instance de `Railcom` avec la broche RX adaptée à votre montage.

L'exemple fourni dans :

```text
railcom_freeRtos_inClass.ino
```

constitue la référence pour la syntaxe exacte du constructeur de la version courante.

La lecture de l'adresse est ensuite volontairement simple :

```cpp
const uint16_t currentAddress = railcom.address();

if (currentAddress != 0)
{
    Serial.printf("Adresse loco : %u\n", currentAddress);
}
```

---

## Plusieurs détecteurs sur un ESP32

L'intérêt principal de l'organisation en classe est de pouvoir créer plusieurs objets indépendants.

Un ESP32 classique possède plusieurs UART matériels. Il est donc possible d'utiliser plusieurs détecteurs RailCom sur un même ESP32, sous réserve :

- d'affecter un UART différent à chaque instance ;
- d'utiliser des GPIO compatibles ;
- de tenir compte du fait que l'UART0 est généralement utilisé pour le téléchargement et le moniteur série.

Historiquement, ce projet a été conçu pour permettre jusqu'à **trois détecteurs RailCom indépendants** sur un ESP32 classique.

Pour une application utilisant également le port série USB comme console, l'utilisation pratique des UART doit naturellement être adaptée au projet.

---

## Matériel RailCom

Cette classe ne réalise pas la détection analogique du retour RailCom directement sur la voie.

Elle attend en entrée le signal UART produit par une **carte détecteur RailCom**.

Documentation du détecteur matériel :

https://www.locoduino.org/spip.php?article334

Le détecteur doit être utilisé avec une centrale DCC générant un **cutout RailCom conforme**.

---

## Niveau logique vers l'ESP32

L'entrée GPIO de l'ESP32 est une entrée **3,3 V**.

Si la carte détecteur est alimentée en 5 V, vérifier que sa sortie UART est adaptée au niveau logique de l'ESP32.

Sur le montage testé, la sortie du 6N137 possède une résistance de pull-up vers 5 V. Une adaptation simple consiste à compléter le montage par un pont permettant d'obtenir environ :

```text
LOW  ≈ 0 V
HIGH ≈ 3,2 à 3,3 V
```

avant l'entrée RX de l'ESP32.

---

## Environnement logiciel

Le projet utilise :

- Arduino Core for ESP32 ;
- FreeRTOS ;
- le driver UART ESP-IDF.

Aucune bibliothèque externe de type `RingBuf` n'est nécessaire dans la version actuelle.

Exemple d'inclusions utilisées par la classe :

```cpp
#include <Arduino.h>
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
```

---

## Avantages de cette version par rapport à l'ancienne implémentation

Ancienne approche :

```text
Serial.available()
    ↓
polling périodique
    ↓
queues intermédiaires
    ↓
buffer circulaire
    ↓
adresse
```

Nouvelle approche :

```text
UART matériel ESP32
    ↓
driver ESP-IDF
    ↓
événement UART
    ↓
parseur RailCom continu
    ↓
validation
    ↓
Railcom::address()
```

Cette organisation apporte :

- moins de polling ;
- moins de dépendances ;
- une meilleure séparation entre acquisition et application ;
- une classe réutilisable ;
- une intégration beaucoup plus simple dans des projets complexes.

---

## Limites actuelles

Cette version est principalement destinée à l'identification de la locomotive.

Elle exploite actuellement :

- le canal 1 RailCom ;
- `ADR1` ;
- `ADR2` ;
- les adresses DCC courtes et longues.

Elle ne décode pas encore les autres informations susceptibles d'être transmises sur RailCom, notamment certaines données du canal 2.

---

## Licence

Ce projet est distribué sous licence :

**GNU General Public License v3.0**

Voir le fichier :

```text
LICENSE
```

---

## Auteur

**Christophe BOBILLE — Locoduino**

https://www.locoduino.org/

---

# 🇬🇧 English

## Overview

**RailCom Detector ESP32 — FreeRTOS / C++ class** is an open-source RailCom® decoder for the ESP32.

The RailCom processing code is encapsulated in a reusable C++ `Railcom` class.

The application can simply query:

```cpp
railcom.address();
```

to retrieve the currently detected locomotive DCC address.

The class handles:

- hardware UART reception at **250,000 baud, 8N1**;
- ESP-IDF UART events;
- FreeRTOS processing;
- continuous byte-stream parsing;
- RailCom 4-out-of-8 decoding;
- channel 1 `ADR1` / `ADR2` decoding;
- short and long DCC addresses;
- address validation;
- automatic loss detection.

`address()` returns the validated DCC address, or `0` when no RailCom locomotive is currently detected.

---

## Project structure

```text
railcom_freeRtos_inClass.ino
Railcom.h
Railcom.cpp
```

`Railcom.h` defines the class interface.

`Railcom.cpp` contains the UART, RailCom decoder and address-validation implementation.

`railcom_freeRtos_inClass.ino` is a minimal example showing how to instantiate and use the class.

---

## Architecture

```text
RailCom detector
      ↓
UART 250 kbaud
      ↓
ESP32 hardware UART
      ↓
ESP-IDF event queue
      ↓
continuous RailCom parser
      ↓
4-out-of-8 decoding
      ↓
ADR1 + ADR2
      ↓
DCC address
      ↓
Railcom::address()
```

UART event boundaries are not considered RailCom datagram boundaries. Bytes are processed as a continuous stream, allowing datagrams split across several UART events to be reconstructed correctly.

---

## Multiple detectors

Because RailCom decoding is encapsulated in a class, several independent `Railcom` objects can be created.

A classic ESP32 provides several hardware UART controllers, allowing multiple RailCom detectors to operate in parallel when UARTs and GPIOs are assigned appropriately.

UART0 is commonly used for programming and the Serial Monitor, so the practical configuration depends on the surrounding application.

---

## Hardware

This software expects the output of an external RailCom detector circuit.

RailCom detector documentation:

https://www.locoduino.org/spip.php?article334

The ESP32 RX input must receive a **3.3 V compatible UART signal**.

The command station must generate a valid RailCom cutout.

---

## Dependencies

- Arduino Core for ESP32
- FreeRTOS
- ESP-IDF UART driver

No external `RingBuf` library is required by the current implementation.

---

## Current scope

The current implementation focuses on locomotive identification using RailCom channel 1:

```text
ADR1
ADR2
```

It supports both short and long DCC addresses.

RailCom channel 2 and other RailCom datagrams are not decoded yet.

---

## License

GNU General Public License v3.0

---

## Author

**Christophe BOBILLE — Locoduino**

https://www.locoduino.org/

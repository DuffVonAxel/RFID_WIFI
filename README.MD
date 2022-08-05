# ESP32 
## WIFI + RFID (RC522)


Arquivo `sdkconfig` contem as configurações básicas (SSID, USER, PASS, etc).


Arquivo `c_cpp_properties.json.txt` (remover a extensão `.txt` do nome) contem o conteudo da pasta `.vscode` na raiz dos projetos.

O projeto em funcionamento deve:
* Mostrar por um navegador web mensagens quando acessado a URI programada;
* Quando um cartão de RFID padrão MILFARE (13,54MHz) estiver ao alcance do leitor MFRC522, o valor sera mostrado.

URI programadas (trocar o `xxx` e o `yyy` pelo valor mostrado pelo ESP32):

http://192.168.xxx.yyy/hello

http://192.168.xxx.yyy/alex

http://192.168.xxx.yyy/fred

http://192.168.xxx.yyy/henrique

http://192.168.xxx.yyy/jose

http://192.168.xxx.yyy/terrao



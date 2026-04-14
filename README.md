# NetPortChk

**Comprobador de conectividad TCP/UDP contra servidores empresariales**

Aplicación MFC para Windows que verifica la accesibilidad de puertos TCP/UDP
contra Domain Controllers, Print Servers, SCCM Full y SCCM Distribution Points
de forma asíncrona, con informe HTML exportable.

Desarrollada con la asistencia de **Claude Sonnet** (Anthropic).  

---

<img width="1002" height="628" alt="image" src="https://github.com/user-attachments/assets/37c5ef00-ce89-4abf-ab2d-d9bf5b54eb1f" />

<img width="820" height="817" alt="image" src="https://github.com/user-attachments/assets/84dda712-c1b3-4acf-9d35-0b787dc53953" />

---

## Puertos por defecto por tipo de servidor

### Domain Controller (DC)
53 TCP/UDP DNS
88 TCP/UDP Kerberos
135 TCP RPC Endpoint Mapper
139 TCP NetBIOS
389 TCP/UDP LDAP
445 TCP SMB
464 TCP/UDP Kerberos Pwd
636 TCP LDAPS
3268 TCP Global Catalog
3269 TCP GC SSL
9389 TCP AD Web Services

### Print Server
80 TCP HTTP
443 TCP HTTPS
445 TCP SMB
515 TCP LPD
631 TCP IPP
9100 TCP RAW

### SCCM Full
80 TCP HTTP
443 TCP HTTPS
445 TCP SMB
135 TCP RPC
8530 TCP WSUS HTTP
8531 TCP WSUS HTTPS
2701 TCP Remote Control
10123 TCP MP alt

### SCCM Distribution Point
80 TCP HTTP
443 TCP HTTPS
445 TCP SMB
8530 TCP WSUS HTTP
8531 TCP WSUS HTTPS

---

## Informe HTML

<img width="1045" height="657" alt="image" src="https://github.com/user-attachments/assets/ea72e39d-2281-44bb-947f-25461b9177a3" />

---

## Iconos

Los iconos de las barras de herramientas han sido obtenidos de:

**[Icons8 · Fluency style](https://icons8.com/icons/fluency)**

---

## Licencia

GNU General Public License v3.0

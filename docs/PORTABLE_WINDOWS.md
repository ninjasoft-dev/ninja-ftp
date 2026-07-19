# NinjaSoft FTP portátil para Windows

Esta distribuição funciona no Windows 10 ou 11 de 64 bits e não exige
instalação.

## Como executar

1. Extraia todo o conteúdo do ZIP para uma pasta local.
2. Abra a pasta `bin`.
3. Execute `NinjaSoftFTP.exe`.

Não execute o programa diretamente de dentro do ZIP: os recursos de interface,
traduções e bibliotecas precisam permanecer na estrutura extraída.

As preferências e os sites cadastrados são armazenados no perfil do usuário do
Windows. Para atualizar, feche o aplicativo e substitua a pasta da versão
anterior pela nova, preservando seus dados no perfil.

## Integridade e segurança

Compare o SHA-256 do ZIP com o arquivo `.sha256` publicado na mesma release.
No PowerShell, execute:

```powershell
Get-FileHash .\NinjaSoft-FTP-1.0.0-Windows-x64-portable.zip -Algorithm SHA256
```

Os binários desta versão ainda não possuem assinatura digital. Por isso, o
Windows pode solicitar confirmação na primeira execução.

## Licença e créditos

O NinjaSoft FTP é distribuído sob a GNU General Public License versão 3 ou
posterior. Ele é baseado no FileZilla Client e não é um produto oficial do
projeto FileZilla. A licença completa, os autores e os créditos acompanham o
pacote.

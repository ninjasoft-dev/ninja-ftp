# NinjaSoft FTP

Cliente desktop de código aberto para **FTP, FTPS e SFTP**, com uma interface
moderna, produtiva e adaptada para português do Brasil.

O NinjaSoft FTP é uma personalização do FileZilla Client voltada ao portfólio
open source da NinjaSoft. O projeto preserva a base robusta do cliente original
e acrescenta uma experiência visual própria, com identidade da marca e melhor
integração ao uso diário no Windows.

## Recursos

- FTP, FTP sobre TLS (FTPS) e SFTP;
- navegação local e remota em dois painéis;
- gerenciador de sites e fila de transferências;
- árvores de diretórios visíveis no layout compacto;
- interface em português do Brasil;
- temas automático, claro e escuro;
- ícones adaptados para fundos claros e escuros.

## Download para Windows

**[Baixar NinjaSoft FTP 1.0.2 para Windows 64
bits](https://github.com/ninjasoft-dev/ninja-ftp/releases/latest/download/NinjaSoft-FTP-1.0.2-Windows-x64-portable.zip)**

Não é necessário instalar: baixe o arquivo ZIP, extraia todo o conteúdo e
execute `bin\NinjaSoftFTP.exe`. As versões anteriores e seus respectivos
checksums permanecem disponíveis na página de
[releases](https://github.com/ninjasoft-dev/ninja-ftp/releases).

O arquivo `.sha256` publicado ao lado do ZIP permite conferir a integridade do
download. Como os binários ainda não possuem assinatura digital, o Windows
pode exibir um aviso de reputação na primeira execução.

## Compilação no Windows

O ambiente de desenvolvimento usa MSYS2 UCRT64. Com as dependências instaladas,
execute na raiz do projeto:

```powershell
.\scripts\build-windows.ps1 -Run
```

O script compila a aplicação, prepara uma versão portátil e, com `-Run`, abre
o executável. Consulte [BUILDING_WINDOWS.md](BUILDING_WINDOWS.md) para instalar
as dependências e conhecer as opções de compilação.

Para gerar o mesmo ZIP distribuído nas releases:

```powershell
.\scripts\package-windows.ps1 -Version 1.0.0
```

## Licença e créditos

Este projeto é distribuído sob a **GNU General Public License, versão 3 ou
posterior**, conforme o arquivo [COPYING](COPYING).

O NinjaSoft FTP é baseado no [FileZilla Client](https://filezilla-project.org/),
software livre desenvolvido por Tim Kosse e colaboradores. Os avisos autorais,
a licença e os créditos das bibliotecas utilizadas são preservados neste
repositório. NinjaSoft FTP não é um produto oficial do projeto FileZilla.

## English

NinjaSoft FTP is an open-source FTP, FTPS and SFTP desktop client based on
FileZilla Client, with a NinjaSoft user experience, Brazilian Portuguese
localization, and automatic, light and dark themes.

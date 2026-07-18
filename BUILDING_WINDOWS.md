# Compilação no Windows

O **Ninja Transfer** usa o ambiente **MSYS2 UCRT64** para gerar um executável
nativo de 64 bits. O fluxo abaixo mantém os artefatos em `build-ucrt64/` e
desativa somente a extensão do Explorer, que não é necessária para desenvolver
ou avaliar a interface do cliente.

A interface inicia em português do Brasil. Se o usuário selecionar outro
idioma nas configurações, a escolha explícita continua sendo respeitada.

O layout compacto aproxima a navegação da interface clássica: as ações são
divididas entre os lados local e remoto, as árvores de diretórios iniciam
visíveis e o registro de mensagens fica como uma aba da fila de transferências.
Na primeira execução após a atualização, esse perfil é aplicado uma única vez;
mudanças feitas pelo usuário depois disso permanecem salvas.

A aparência inicia em **Automático**, acompanhando a preferência de cores do
sistema operacional. Na tela principal, o seletor no canto superior direito
alterna entre **Claro** e **Escuro** imediatamente. O modo automático continua
disponível em **Editar > Configurações > Interface > Temas**. A escolha de
ícones permanece independente da aparência da aplicação.

## Uso rápido

Na raiz do projeto, execute:

```powershell
.\scripts\build-windows.ps1 -Run
```

O parâmetro `-Run` abre a aplicação depois da compilação. Sem ele, o script
apenas compila e prepara a versão portátil.

O executável fica em:

```text
build-ucrt64\stage\ucrt64\bin\filezilla.exe
```

Depois de alterar o código, repita o mesmo comando. O `make` recompila apenas
o que mudou. Use `-Reconfigure` somente depois de modificar arquivos do
Autotools ou opções de configuração:

```powershell
.\scripts\build-windows.ps1 -Reconfigure -Run
```

## Dependências

O script espera o MSYS2 em `C:\msys64`. Em outra máquina, atualize o ambiente
com `pacman -Syu` e instale os pacotes abaixo no PowerShell:

```powershell
& C:\msys64\usr\bin\pacman.exe -S --needed base-devel `
  mingw-w64-ucrt-x86_64-gcc `
  mingw-w64-ucrt-x86_64-autotools `
  mingw-w64-ucrt-x86_64-boost `
  mingw-w64-ucrt-x86_64-cppunit `
  mingw-w64-ucrt-x86_64-fzssh `
  mingw-w64-ucrt-x86_64-gettext-tools `
  mingw-w64-ucrt-x86_64-gnutls `
  mingw-w64-ucrt-x86_64-libfilezilla `
  mingw-w64-ucrt-x86_64-pkgconf `
  mingw-w64-ucrt-x86_64-sqlite3 `
  mingw-w64-ucrt-x86_64-wxwidgets3.2-msw
```

Se a atualização trocar o runtime do MSYS2, feche seus terminais MSYS2 e
execute `pacman -Syu` novamente antes de instalar as dependências.

## Escopo deste build

- Interface gráfica, FTP, FTPS, SFTP, gerenciador de sites e traduções estão
  habilitados.
- Os modos automático, claro e escuro abrangem janelas, menus, campos, árvores,
  listas e diálogos criados durante a execução.
- Os modos claro e escuro compartilham a identidade visual do Ninja Translator:
  superfícies em camadas, bordas discretas e roxo como cor de ação. A organização
  compacta em dois painéis continua priorizando a produtividade.
- A verificação automática de atualização fica desativada no executável de
  desenvolvimento.
- A extensão de menu do Explorer não é compilada. Ela exige também o toolchain
  MinGW de 32 bits e não interfere na aparência ou no funcionamento do cliente.
- `build-ucrt64/` não deve ser versionado.

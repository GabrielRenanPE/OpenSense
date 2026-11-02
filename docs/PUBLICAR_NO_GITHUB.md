# Publicar o OpenSense no GitHub — guia do Gabriel

Este pacote contém o projeto 0.6.0 preparado para publicação. Nenhum repositório foi criado e nenhum arquivo foi enviado à sua conta automaticamente.

## 1. Extrair e conferir a pasta

Extraia OpenSense-GitHub.zip. A pasta OpenSense-GitHub deve conter README.md, CMakeLists.txt, src/, drivers/, docs/ e .github/. As pastas e arquivos iniciados por ponto também fazem parte do projeto; no Dolphin, use Alt+. para mostrá-los, se necessário.

Abra o terminal nessa pasta. Não execute os comandos de Git na sua pasta pessoal inteira. Envie os arquivos extraídos, não somente o ZIP.

O pacote inclui os fontes do driver, patch, COPYING, ORIGEM.txt, Makefile, Kbuild, dkms.conf e scripts. Não inclua módulos .ko compilados, build/, senhas ou diagnósticos pessoais.

## 2. Escolhas já preparadas

| Campo | Conteúdo sugerido |
| --- | --- |
| Owner | GabrielRenanPE |
| Repository name | OpenSense |
| Description | Native Linux control center for compatible Acer laptops: fans, battery protection and GPU modes. Built with C++ and Qt 6. |
| Visibility | Public, quando estiver pronto para compartilhar |
| Website | Deixe vazio se não houver site próprio |
| Topics | linux, acer, nitro, fan-control, battery, qt6, cpp, cachyos, dkms |
| Licença do pacote | GPL-2.0-or-later |
| Primeira tag | v0.6.0 |
| Tipo da primeira release | Pre-release (beta) |

A licença GPL-2.0-or-later foi incluída nesta preparação para permitir colaboração e redistribuição nos termos dela. Leia LICENSE e NOTICE.md antes de publicar. Os avisos originais do driver não podem ser substituídos por uma atribuição de autoria exclusiva ao OpenSense.

## 3. Instalar Git e GitHub CLI

No CachyOS:

```bash
sudo pacman -Syu --needed git github-cli
```

Uma atualização do sistema pode exigir reinício; estes comandos de publicação não instalam o driver.

## 4. Autenticar sua conta

```bash
gh auth login --hostname github.com --git-protocol https --web
gh auth setup-git
gh auth status
```

Siga a autenticação pelo navegador e confirme que a conta ativa é GabrielRenanPE. Não cole tokens em arquivos do projeto ou nesta conversa. A senha comum da conta não deve ser usada como senha de Git por HTTPS.

## 5. Criar o histórico local

Dentro da pasta extraída:

```bash
pwd
git init -b main
git config user.name "Gabriel Renan Silva Martins Guimarães"
```

Em GitHub → Settings → Emails, copie o endereço de commit que deseja usar. Para privacidade, copie exatamente o endereço noreply mostrado pelo GitHub; não invente o identificador numérico. No comando abaixo, substitua o texto de exemplo pelo endereço escolhido:

```bash
git config user.email "COLE_AQUI_SEU_EMAIL_DE_COMMIT"
```

Confira o conteúdo antes do primeiro commit:

```bash
git add .
git status --short
git diff --cached --stat
git commit -m "Prepare OpenSense 0.6.0 beta for public release"
```

O .gitignore exclui artefatos comuns, mas não substitui sua revisão. A configuração de nome/email acima é local a este repositório.

## 6. Criar e publicar o repositório

Se GabrielRenanPE/OpenSense ainda NÃO existir, este comando cria o repositório público e envia o commit:

```bash
gh repo create GabrielRenanPE/OpenSense --public --source=. --remote=origin --push --description "Native Linux control center for compatible Acer laptops: fans, battery protection and GPU modes. Built with C++ and Qt 6."
```

Depois:

```bash
gh repo view --web
```

Se preferir revisar em privado, troque --public por --private antes da criação e altere a visibilidade depois na página do repositório.

### Se o repositório já existir

Não repita a criação. Se ele estiver vazio, conecte o remoto e envie:

```bash
git remote add origin https://github.com/GabrielRenanPE/OpenSense.git
git push -u origin main
```

Se origin já existir, confira `git remote -v` antes de mudar qualquer configuração. Se o repositório remoto já tiver commits, não use force push: clone-o em outra pasta, copie os arquivos preparados para ela, revise o diff e crie um commit normal.

### Alternativa: criar pelo site

Use New repository, owner GabrielRenanPE e nome OpenSense. Deixe desmarcadas as opções de adicionar README, .gitignore e licença, pois o pacote já inclui esses arquivos. Depois execute os comandos do bloco para repositório vazio acima. Use apenas um dos métodos de criação.

## 7. Organizar a página

No painel About, adicione a descrição e os tópicos sugeridos. Deixe Issues habilitado para os relatos de problemas. O README aparece automaticamente porque está na raiz. .github/ISSUE_TEMPLATE e pull_request_template.md fornecem os modelos.

Na aba Actions, confira o fluxo “Build e testes”. Ele compila e testa o aplicativo; não instala nem valida fisicamente o driver. O YAML foi preparado e os testes locais passaram, mas a primeira execução nos runners do GitHub ainda precisa ser verificada.

Em Settings → Code security, procure a opção de private vulnerability reporting, se disponível, e habilite-a para receber relatos privados. O arquivo SECURITY.md explica o caminho alternativo caso ela não esteja ativa.

## 8. Criar a primeira release

Após conferir o README e o resultado de Actions:

```bash
git tag -a v0.6.0 -m "OpenSense 0.6.0 beta"
git push origin v0.6.0
gh release create v0.6.0 --verify-tag --prerelease --title "OpenSense v0.6.0 — beta" --notes-file docs/RELEASE_v0.6.0.md
```

O GitHub disponibiliza arquivos de código-fonte da tag; eles já incluem drivers/ e documentação. Não é necessário anexar .ko ou executáveis da sua máquina. O texto da release já informa limitações, instalação e versão do driver.

Se v0.6.0 já existir, não sobrescreva a tag publicada: confira seu conteúdo e use uma próxima versão para novas mudanças.

## 9. Publicar atualizações futuras

```bash
git status
git add .
git diff --cached --stat
git commit -m "Describe the changes made"
git push
```

Antes de uma nova release, atualize a versão em CMakeLists.txt e src/main.cpp, acrescente CHANGELOG.md e execute os testes. Use uma nova tag. Quando mudar o código do driver, revise também sua versão DKMS, compatibilidade, fontes e documentação; não republique um código diferente sob a mesma versão instalada sem planejar a atualização.

## O que está no pacote

- Código do aplicativo e auxiliares, recursos e testes.
- Driver DKMS completo com licenciamento e procedência.
- README, LICENSE, NOTICE, CHANGELOG, CONTRIBUTING e SECURITY.
- Guias de instalação, arquitetura, compatibilidade e recuperação.
- Modelos de bugs, sugestões e pull requests.
- Workflow de compilação/testes com permissão de leitura.
- Capturas reais do ambiente de teste, identificadas como tal.

## Referências oficiais consultadas

- [Importar código local](https://docs.github.com/en/migrations/importing-source-code/using-the-command-line-to-import-source-code/adding-locally-hosted-code-to-github)
- [gh auth login](https://cli.github.com/manual/gh_auth_login)
- [gh repo create](https://cli.github.com/manual/gh_repo_create)
- [gh release create](https://cli.github.com/manual/gh_release_create)
- [GNU GPL versão 2](https://www.gnu.org/licenses/old-licenses/gpl-2.0.html)

Os nomes de menus do GitHub podem mudar; os comandos acima seguem a documentação consultada na preparação.

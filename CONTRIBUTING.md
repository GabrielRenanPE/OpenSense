# Como contribuir

Obrigado por ajudar a melhorar o OpenSense. Correções, documentação, traduções e relatos de compatibilidade são bem-vindos.

1. Abra uma issue descrevendo o problema ou a proposta. Informe modelo, kernel, versão do OpenSense, distribuição e sessão gráfica quando relevantes.
2. Faça um fork e crie uma branch curta e descritiva.
3. Compile com CMake e execute ctest conforme README.
4. Inclua testes proporcionais à mudança. Alterações de hardware precisam separar teste simulado de teste físico.
5. Envie um pull request explicando problema, solução, validação e limitações.

Não envie binários de build, .ko, logs privados, chaves ou credenciais. Não amplie a autorização Polkit para shells/comandos arbitrários. Preserve avisos de autoria e licenças. Contribuições devem ser oferecidas sob GPL-2.0-or-later, respeitando direitos de terceiros.

Para suporte a um novo modelo, não copie somente a entrada DMI. Documente interfaces, fonte técnica e comportamento verificado, com um caminho de recuperação. Não remova a restrição de kernel sem validar as APIs.

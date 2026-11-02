# Solução de problemas

## cmake não encontrado

Instale as dependências de INSTALACAO.md. O comando opensense só fica disponível após a instalação concluir.

## Manual indisponível

Verifique o módulo e os atributos, sem escrever neles:

```bash
uname -r
bash drivers/status.sh
```

fan1_input/fan2_input são leituras. O modo Manual precisa de uma interface de controle, como pwm1/pwm2. Não force parâmetros de outro modelo. Inclua a versão do aplicativo, modelo e kernel ao abrir uma issue.

## Senha em cada ajuste

Feche a versão antiga, instale com --app-only e reabra. Confira se o opensense-fan-helper e a política Polkit foram instalados. A isenção padrão vale apenas para resfriamento em sessão local ativa; regras locais podem prevalecer. GPU/bateria e perfis do power-profiles-daemon têm políticas próprias.

## Falha do initramfs ou hook ausente

Não reinicie enquanto houver erro. O problema pode estar na configuração do gerador de initramfs da distribuição; reinstalar a interface não o resolve. Preserve a saída completa e verifique qual integração de boot está instalada. Não remova hooks de Btrfs, Limine ou criptografia às cegas. A remoção do driver também regenera initramfs e pode falhar pelo mesmo motivo.

## Sobreposição ou animação incorreta

Ative temporariamente Reduzir animações e registre versão, resolução, escala e tipo de sessão. A versão 0.5.0 removeu efeitos de opacidade da pilha de páginas. As animações da 0.6.0 são locais. Confirme que encerrou o processo antigo na bandeja antes de abrir a versão atual.

## Bateria sem controle de 80%

A leitura de carga não significa suporte ao limite. O driver acer_wmi_battery usado pelo criador é separado do driver de ventoinhas. Não grave em atributos arbitrários. Verifique o diagnóstico da aba Ajuda e a interface disponibilizada pelo seu kernel.

Antes de compartilhar diagnósticos, remova números de série, nome do computador, caminhos pessoais e outros dados que não sejam necessários.

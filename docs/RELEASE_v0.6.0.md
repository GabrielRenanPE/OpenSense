# OpenSense v0.6.0 — beta

Primeiro pacote preparado para publicação do OpenSense: uma central nativa para notebooks Acer no Linux.

## Incluído

- Interface Qt 6 exclusivamente dark, monitoramento e animações dos modos.
- Ventoinhas: Eco, Automático, Desempenho e Manual 25–100%, conforme suporte do driver.
- Saúde estimada e limite de bateria de 80%, quando disponível.
- Integração com EnvyControl para modo híbrido/integrado.
- Fontes e empacotamento DKMS do driver específico Acer Nitro ANV15-51.
- Documentação de instalação, recuperação e contribuição.

## Atualizar

Feche o aplicativo, inclusive na bandeja, e execute `bash install.sh --app-only` na pasta do projeto. Isso mantém o driver atual.

## Limitações

Versão beta. O driver incluído é restrito ao ANV15-51 e à série de kernel 7.3.x; teste físico relatado em 7.3.0-4-cachyos. Não oferece compatibilidade universal. O modo Manual não tem supervisão térmica contínua. Consulte README, drivers/README.md e VALIDACAO.md antes de instalar.

Os testes locais de núcleo, escopo e interface passaram. O GitHub Actions incluído ainda precisa executar no repositório público. A validação local não equivale a teste de boot ou hardware.

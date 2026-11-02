# Compatibilidade

| Componente | Situação |
| --- | --- |
| Interface Qt 6 / C++17 | Compilada e testada offscreen em Linux; não depende do KDE |
| Notebook de referência | Acer Nitro V ANV15-51; produto DMI exato Nitro ANV15-51 |
| Ambiente relatado pelo criador | CachyOS, KDE Plasma / Wayland |
| Kernel do teste físico do driver | 7.3.0-4-cachyos, conforme relato do criador |
| Driver DKMS 0.1.0 | Build limitado a 7.3.x; novas revisões dependem de teste |
| LTS 6.18 e outras séries | Módulo customizado não instalado; permanece módulo da distribuição |
| Bateria de 80% | Funcionamento relatado via acer_wmi_battery; módulo separado não incluído |
| Outros notebooks / distribuições | Não validados; recursos dependem das interfaces disponíveis |

O suporte de uma interface não garante suporte de todas as outras. Leituras de RPM não comprovam que PWM está disponível. O controle pode aparecer desabilitado quando a interface ou o auxiliar estão ausentes.

## Limites atuais

- Não há promessa de compatibilidade universal ou com todo kernel futuro.
- Não há curva térmica automática própria nem supervisão contínua no modo Manual.
- Animações são decorativas; RPM e estados são lidos separadamente.
- A proteção não descarrega imediatamente a bateria quando ela já está acima de 80%.
- Persistência do limite depende do firmware/driver; não é garantida pelo aplicativo.
- Troca de modo gráfico exige reinício; saídas de vídeo ligadas à NVIDIA podem ficar indisponíveis no modo integrado.
- Instalação do módulo com lockdown/Secure Boot exige um fluxo de assinatura não oferecido por este instalador. Não desative proteções automaticamente.
- O fluxo de CI não compila o driver para o kernel do runner nem testa o hardware.

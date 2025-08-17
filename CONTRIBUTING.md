# Contribuindo para o Blender

Obrigado por considerar contribuir com o Blender. Este documento fornece uma visão geral do fluxo de trabalho utilizado no projeto.

## Requisitos gerais
- Todo código deve ser compatível com a [Licença Pública Geral GNU v3](https://www.gnu.org/licenses/gpl-3.0.html).
- Realize pequenas e focadas mudanças por commit.

## Estilo de código
- Python: utilize [Black](https://black.readthedocs.io/en/stable/) com `line-length` de 120 caracteres.
- C/C++: siga as convenções descritas em `doc/code_style` (TODO: documento ainda não existe).

## Testes
- Execute `make test` antes de enviar uma alteração. Alguns testes exigem dependências externas.

## Fluxo de contribuição
1. Faça o fork do repositório e crie commits em um branch.
2. Execute os testes e assegure-se de que passam.
3. Abra um Pull Request descrevendo as mudanças e resultados dos testes.

Para dúvidas adicionais, participe do [Developer Forum](https://devtalk.blender.org).

# Documentação do Repositório Blender

Este documento fornece uma visão geral dos principais componentes do repositório e como se relacionam.

## Estrutura de Diretórios

- `build_files/`: scripts e configurações para gerar builds em diferentes plataformas.
- `doc/`: documentação interna, manuais e guias especializados.
- `extern/` e `lib/`: dependências externas e bibliotecas pré-compiladas.
- `release/`: arquivos de distribuição e materiais de lançamento.
- `scripts/`: scripts Python utilizados em tempo de execução (add-ons, módulos, presets).
- `source/`: código-fonte principal do Blender (C/C++ e Python).
- `tests/`: suíte de testes automatizados.
- `tools/`: utilitários para desenvolvimento, integração e revisão de código.

## Documentação

A pasta `doc/` contém documentação voltada a desenvolvedores e mantenedores.
Alguns subdiretórios relevantes:

- `blender_file_format/`: especificação do formato de arquivo do Blender.
- `guides/`: guias históricos e materiais legados.
- `license/`: informações sobre licenciamento.
- `manpage/`: script para geração da página de manual (`man blender`).
- `python_api/`: documentação gerada da API Python.

## Scripts

Os scripts são organizados em subpastas como `addons_core/`, `modules/` e `startup/`.
Consulte [scripts/README.md](scripts/README.md) para mais detalhes e diretrizes de contribuição.

## Testes

Os testes do projeto estão em `tests/`, com suporte a testes de unidade, integração e desempenho.
Para executar todos os testes disponíveis:

```bash
make test
```

Alguns testes requerem dados ou a compilação prévia do Blender.

## Ferramentas de Desenvolvimento

Utilitários em `tools/` auxiliam na revisão de commits, linting e integração com sistemas externos.
É possível executá-los a partir da raiz do repositório.

## Contribuindo

Instruções de estilo, fluxo de trabalho e ferramentas recomendadas estão descritas em [CONTRIBUTING.md](CONTRIBUTING.md).

## Licença

Blender é distribuído sob a [GNU General Public License, versão 3](https://www.blender.org/about/license).
Arquivos individuais podem ter licenças compatíveis, especificadas em seus cabeçalhos.

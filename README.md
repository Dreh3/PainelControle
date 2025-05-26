# PainelControle
Repositório destinado ao desenvolvimento da Tarefa sobre utilização de Semáforos em Sistemas Multitarefas da Fase da Residência em Software Embarcado - Embarca Tech TIC 37

__Responsável pelo desenvolvimento:__
Andressa Sousa Fonseca

## Componentes utilizados
1) Matriz de LEDs
2) Display OLED
3) LEDs RGB
4) Buzzer
5) Botões

## Destalhes do projeto

O projeto utiliza semáforos para registrar eventos. Os eventos são entrada e saída de passageiros de um ônibus. Há um semáforo de contagem para registrar as entradas através da interrupção no botão A e outro para registrar as saídas com o Botão B. Cada vez que um evento é detectado no semáforo, a task responsável modifica a variável de quantidade de passageiros e tenta adquirir o mutex para atualizar o display imediamente após a modificação. Por sua vez, o botão do Joystick aciona o semáforo binário, que será capturado pela taskReset, responsável por  zerar a contagem. Outras funcionalidades inclue buzzer e matriz de leds.
<br>
1) Funcionalidades do buzzer:<br>
- Emite um beep quando o limite de passageiros é atingido.
- Emite duplo beep quando o sistema é resetado.
<br>
2) Funcionalidades da Matriz de Leds:<br>
- Mostra a contagem visual de passageiros com os leds correspondentes acesos.
<br>
3) Funcionalidades da LEDs RGB:<br>
Exibe uma cor para cada intervalo de contagem:<br>
- Azul: Contagem em 0
- Verde: entre 0 e o máximo menos 2;
- Amarelo: contagem em um a menos que o limite;
- Vermelho: contagem no limite máximo;
   

   


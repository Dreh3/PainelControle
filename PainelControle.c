//Código desenvolvidos por Andressa Sousa Fonseca

/*
*O projeto utiliza semáforos para registrar eventos. Os eventos são entrada e saída de passageiros de um ônibus.
*Há um semáforo de contagem para registrar as entradas através da interrupção no botão A e outro para registrar 
*as saídas com o Botão B. Cada vez que um evento é detectado no semáforo, a task responsável modifica a variável
*de quantidade de passageiros e tenta adquirir o mutex para atualizar o display imediamente após a modificação.
*Por sua vez, o botão do Joystick aciona o semáoro binário, que será capturado pela taskReset, responsável por 
*zerar a contagem.
*/

//Importando bilbiotecas importantes
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"
#include "hardware/timer.h"
#include "hardware/i2c.h"
#include "lib/ssd1306.h"
#include "lib/font.h"
#include "lib/matriz.h"

#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"
#include "semphr.h"

//Configurações para comunicação I2C
#define I2C_PORT i2c1 //Comucação I2C
#define I2C_SDA 14
#define I2C_SCL 15
#define endereco 0x3C
ssd1306_t ssd; 
bool cor = true;

uint passageiros_ativos = 0;  //Variável global para os ocupantes

//Pino do Joystick
#define ButtonJoy 22
//pino botão A
#define ButtonA 5
//Pino do botão
#define ButtonB 6

#define MAX_USUARIOS 25     //Quantidade máxima de poltronas no ônibus

bool reset = false;
bool limite_atingido = false;

//Criando mutex
SemaphoreHandle_t xMutex; //Mutex para o display OLED

//Criando semaforos para o eventos de entrada e de saida
SemaphoreHandle_t xSemBinario; //semaforo binário
SemaphoreHandle_t xSemContagemE; //semaforo para o evento de entrada
SemaphoreHandle_t xSemContagemS; //semaforo para o evento de saida

BaseType_t xHigerPriorityTaskWoken = pdFALSE;  //para possibilitar a troca de contexto após acionar o semáforo

//Função para mostrar a mensagem padrão que vai ser chamada nas tasks principais quando for preciso atualizar o display
void modelo_Display(){
    //Variáveis para salvar os valores em strings
    char contagem_atual[3];
    char contagem_maxima[9];

    //  Atualiza o conteúdo do display com as informações necessárias
    ssd1306_fill(&ssd, !cor);                          // Limpa o display
    ssd1306_rect(&ssd, 3, 1, 122, 61, cor, !cor);      // Desenha um retângulo ok
    ssd1306_line(&ssd, 3, 13, 123, 13, cor);           // Desenha uma linha ok
    ssd1306_line(&ssd, 3, 23, 123, 23, cor);           // Desenha uma linha
    ssd1306_line(&ssd, 3, 33, 123, 33, cor);           // Desenha uma linha
    ssd1306_draw_string(&ssd,"Onibus Rota A", 9, 5); // Desenha uma string
    ssd1306_draw_string(&ssd,"P Ocupadas:", 4, 15);               // Desenha uma string
    ssd1306_draw_string(&ssd,"Total:", 4, 25);               // Desenha uma string

    //Exibe a quantidade usuarios
    sprintf(contagem_atual,"%d",passageiros_ativos);
    ssd1306_draw_string(&ssd, contagem_atual, 95, 15);              // Desenha uma string
    sprintf(contagem_maxima,"%d",MAX_USUARIOS);
    ssd1306_draw_string(&ssd, contagem_maxima, 95, 25);              // Desenha uma string

    //Verifica a quantidade para exibir a mensagem correspondente
    if(passageiros_ativos==0){
        ssd1306_draw_string(&ssd,"Onibus",37,40);
        ssd1306_draw_string(&ssd,"Vazio!",37,50); 
    }else if(passageiros_ativos>=0 && passageiros_ativos<=MAX_USUARIOS-2){
        ssd1306_draw_string(&ssd,"Poltronas",25,40);
        ssd1306_draw_string(&ssd,"disponiveis!",13,51); 
    }else if(passageiros_ativos==MAX_USUARIOS-1){
        ssd1306_draw_string(&ssd,"Ultima poltrona",1,40);
        ssd1306_draw_string(&ssd,"disponivel.",21,51); 
    }else{
        limite_atingido = true;
        ssd1306_draw_string(&ssd,"Lotado!",33,40);
        ssd1306_draw_string(&ssd,"Sem poltronas.",5,51); 
    };

};

//Task para o controle de entrada - Ela verifica o semáforo de contagem e incrementa caso haja eventos no vetor
void vTaskEntrada(){

    while(true){

        //Tempo de espera pequeno para garantir atualizações mais rápidas
        if(xSemaphoreTake(xSemContagemE, pdMS_TO_TICKS(50))==pdTRUE){  //Verifica se foi possível adquiri o semáforo
            if(passageiros_ativos<MAX_USUARIOS){
                passageiros_ativos++;                                  //Incrementa a quantidade de passageiros
            };
            
            if(xSemaphoreTake(xMutex, portMAX_DELAY)==pdTRUE){         //Tenta ter acesso ao Mutex

                //chama a função da mensagem padrão
                modelo_Display();
                ssd1306_send_data(&ssd); //Atualiza o display

                xSemaphoreGive(xMutex); //Libera o mutex, assim uma outra task pode ter acesso ao display
            }; 
        };
        vTaskDelay(pdMS_TO_TICKS(250));
    };
};

//Task para o controle de saída - Ela verifica o semáforo de contagem e decrementa, caso haja eventos no vetor
void vTaskSaida(){   

    while(true){
        if(xSemaphoreTake(xSemContagemS, pdMS_TO_TICKS(50))==pdTRUE){
            if(passageiros_ativos>0){
                passageiros_ativos--;
            };
        
            if(xSemaphoreTake(xMutex, portMAX_DELAY)==pdTRUE){        
                //chama a função da mensagem padrão
                modelo_Display();
                ssd1306_send_data(&ssd);

                xSemaphoreGive(xMutex);
            }; 
        };
        vTaskDelay(pdMS_TO_TICKS(250));
    };
};

//Faz o controle do reset do sistema - Só reset se o semáforo binário for liberado com a interrupção do Botão do Joystick
void vTaskReset(){
    while(true){
        if(xSemaphoreTake(xSemBinario, portMAX_DELAY)==pdTRUE){ //Caso consiga acessar o semáforo binário
            passageiros_ativos = 0;                             //O sistema é resetado
            reset = true;                                       //Variável de reset para controle do beep do buzzer
            if(xSemaphoreTake(xMutex, portMAX_DELAY)==pdTRUE){
                
                //chama a função da mensgem padrão
                modelo_Display();
                ssd1306_send_data(&ssd);                        //Atualiza o display com a nova contagem

                xSemaphoreGive(xMutex);
            };
        };
    };
};

//Função de interrupção para os botões
uint32_t tempo_anterior = 0;
void InterrupcaoBotao(uint gpio, uint32_t events){
    uint32_t tempo_atual = to_ms_since_boot(get_absolute_time());
    if(tempo_atual - tempo_anterior >= 300){                        //Tratamento de debouncing
        tempo_anterior = tempo_atual;
        if(gpio==ButtonJoy){                                        //Verifica se a interrupção foi gerada pelo botão Joystick
            xHigerPriorityTaskWoken = pdFALSE;
            xSemaphoreGiveFromISR(xSemBinario, &xHigerPriorityTaskWoken); //Libera o semáforo binário
            portYIELD_FROM_ISR(xHigerPriorityTaskWoken);
        }else if(gpio==ButtonA){                                    //Verifica se a interrupção foi gerada pelo botão A
            xHigerPriorityTaskWoken = pdFALSE;
            xSemaphoreGiveFromISR(xSemContagemE, &xHigerPriorityTaskWoken); //Adiciona um evento ao semáforo de contagem de entrada
            portYIELD_FROM_ISR(xHigerPriorityTaskWoken);
        }else if(gpio==ButtonB){                                    //Verifica se a interrupção foi gerada pelo botão B
            xHigerPriorityTaskWoken = pdFALSE;
            xSemaphoreGiveFromISR(xSemContagemS, &xHigerPriorityTaskWoken); //Adiciona um evento ao semáforo de contagem de saída
            portYIELD_FROM_ISR(xHigerPriorityTaskWoken);
        };
    };
};

//Task do buzzer
#define WRAP 65535
#define Buzzer 21

void vTaskBuzzer(){

    uint dados_contagem;  //Cria a variável para armazenar os dados da task

    uint sons_freq[] = {880,784,740,659};   //Frequência para os sons de beep

    //Configurações de PWM
    uint slice;
    gpio_set_function(Buzzer, GPIO_FUNC_PWM);   //Configura pino do led como pwm
    slice = pwm_gpio_to_slice_num(Buzzer);      //Adiquire o slice do pino
    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, clock_get_hz(clk_sys) / (4400 * WRAP));
    pwm_init(slice, &config, true);
    pwm_set_gpio_level(Buzzer, 0);              //Determina com o level desejado - Inicialmente desligado
  
    while(1){

        
            if(reset){ //Se o reset foi ativado, o buzzer emite um beep duplo
                reset=false;    //Garante que o beep não repita indefinidamente
                for(int i =0; i<2; i++){                //Laço de repetição para modificar as frequência do beep
                    pwm_config_set_clkdiv(&config, clock_get_hz(clk_sys) / (sons_freq[i] * WRAP));
                    pwm_init(slice, &config, true);
                    pwm_set_gpio_level(Buzzer, WRAP/2); //Level metade do wrap para volume médio
                    vTaskDelay(pdMS_TO_TICKS(100));     //pausa entre sons
                };
                pwm_set_gpio_level(Buzzer, 0);          //Desliga o buzzer ao final do ciclo
                vTaskDelay(pdMS_TO_TICKS(100));
            }else if (passageiros_ativos==MAX_USUARIOS && limite_atingido){ // Caso o limite maximo seja atingido, emite um beep uma única vez
                pwm_config_set_clkdiv(&config, clock_get_hz(clk_sys) / (sons_freq[0] * WRAP));
                pwm_init(slice, &config, true);
                pwm_set_gpio_level(Buzzer, WRAP/2);
                vTaskDelay(pdMS_TO_TICKS(100));
                pwm_set_gpio_level(Buzzer, 0);          
                vTaskDelay(pdMS_TO_TICKS(100));
                limite_atingido = false;
        }else{
            pwm_set_gpio_level(Buzzer, 0);    //Nos outros momentos, mantem o buzzer desligado          
            vTaskDelay(pdMS_TO_TICKS(100));
        };
    };
};

//Configurações da matriz
PIO pio = pio0; 
uint sm = 0;    

void vTaskMatrizLeds(){

    //Configurações para matriz de leds
    uint offset = pio_add_program(pio, &pio_matrix_program);
    pio_matrix_program_init(pio, sm, offset, MatrizLeds, 800000, IS_RGBW);

    //Padrão para desligar led da matriz
    COR_RGB off = {0.0,0.0,0.0};
    //Vetor para variar entre os estados da matriz
    COR_RGB pessoa_cor = {0.004,0.004,0.001};

    //matriz que será modificada no loop
    Matriz_leds ocupacao_visual = {{off,off,off,off,off},{off,off,off,off,off},{off,off,off,off,off},{off,off,off,off,off},{off,off,off,off,off}};

    //Essa variável recebe o valor de passageiros e vai decrementa a medida que os leds são ligados. Assim, exibe a quantidade correta
    uint controle_contagem = 0;

    //Inicia com a matriz desligada
    acender_leds(ocupacao_visual);

    while(true){

        controle_contagem = passageiros_ativos;  //A variável armazena a quantidade passageiros atual
        
        //For para percorrer todas as posições da matriz
        for(int i = 0; i <5; i++){
            for(int j=0; j<5;j++){
                if(controle_contagem>0){                //Caso, a variável de contagem seja maior que zero
                    ocupacao_visual[i][j]=pessoa_cor;   //a posição atual do laço recebe o padrão da cor definida acima
                    controle_contagem --;               //Com isso, a variável é decrementada
                } else if(controle_contagem<=0){        //Caso seja zero, não há mais passageiros a serem registrados
                    ocupacao_visual[i][j]=off;          //Assim, a posição recebe o padrão off
                }; 
            };
        };
        
        acender_leds(ocupacao_visual);                  //Atualiza a matriz
        sleep_ms(100);
    };
};


//Pinos leds
#define led_Green 11
#define led_Blue 12   
#define led_Red 13

void vTaskLEDS(){
    
    uint dados_contagem;

    //Inicializando pinos
    gpio_init(led_Green);
    gpio_init(led_Red);
    gpio_init(led_Blue);
    gpio_set_dir(led_Green, GPIO_OUT);
    gpio_set_dir(led_Red, GPIO_OUT);
    gpio_set_dir(led_Blue, GPIO_OUT);

    gpio_put(led_Green,0);
    gpio_put(led_Red,0);
    gpio_put(led_Blue,0);

    while(true){

        
        if(passageiros_ativos==0){      //Led azul quando não houver passageiros
            gpio_put(led_Blue,1);
            gpio_put(led_Green,0);
            gpio_put(led_Red,0);
        }else if(passageiros_ativos>=0 && passageiros_ativos<=MAX_USUARIOS-2){  //Led Verde para indicar que há passageiros
            gpio_put(led_Green,1);                                              //e não atingiu o máximo ainda
            gpio_put(led_Red,0);
            gpio_put(led_Blue,0);
        }else if(passageiros_ativos==MAX_USUARIOS-1){                           //Led Amarelo para indicar que só uma poltrona 
            gpio_put(led_Green,1);                                              //disponível
            gpio_put(led_Red,1);
            gpio_put(led_Blue,0);
        }else if (passageiros_ativos==MAX_USUARIOS){                            //Led Vermelho indicando que todas as poltronas estão 
            gpio_put(led_Red,1);                                                //ocupadas
            gpio_put(led_Blue,0);
            gpio_put(led_Green,0);
        };
        vTaskDelay(pdMS_TO_TICKS(200));
    };
};

int main()
{
    stdio_init_all();

    //configurando botão B
    gpio_init(ButtonB);
    gpio_set_dir(ButtonB, GPIO_IN);
    gpio_pull_up(ButtonB);

    //configurando botão do joystick
    gpio_init(ButtonJoy);
    gpio_set_dir(ButtonJoy, GPIO_IN);
    gpio_pull_up(ButtonJoy);

    //Configurando Botão A
    gpio_init(ButtonA);
    gpio_set_dir(ButtonA, GPIO_IN);
    gpio_pull_up(ButtonA);
    
    //Configura interrupção para o botão
    gpio_set_irq_enabled_with_callback(ButtonA, GPIO_IRQ_EDGE_FALL,true, InterrupcaoBotao);
    gpio_set_irq_enabled_with_callback(ButtonB, GPIO_IRQ_EDGE_FALL,true, InterrupcaoBotao);
    gpio_set_irq_enabled_with_callback(ButtonJoy, GPIO_IRQ_EDGE_FALL,true, InterrupcaoBotao);

    //Configurar o display aqui e mensagens padrão
    // I2C Initialisation. Using it at 400Khz.
    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);                    // Set the GPIO pin function to I2C
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);                    // Set the GPIO pin function to I2C
    gpio_pull_up(I2C_SDA);                                        // Pull up the data line
    gpio_pull_up(I2C_SCL);                                        // Pull up the clock line
    ssd1306_init(&ssd, WIDTH, HEIGHT, false, endereco, I2C_PORT); // Inicializa o display
    ssd1306_config(&ssd);                                         // Configura o display
    ssd1306_send_data(&ssd);                                      // Envia os dados para o display
    
    //Inicializa com o modelo padrão
    modelo_Display();
    ssd1306_send_data(&ssd);

    //criar mutexes e semaforos
    xMutex = xSemaphoreCreateMutex();
    xSemBinario = xSemaphoreCreateBinary();
    xSemContagemE = xSemaphoreCreateCounting(MAX_USUARIOS,0);
    xSemContagemS = xSemaphoreCreateCounting(MAX_USUARIOS,0);

    //criando tasks
    xTaskCreate(vTaskEntrada, "Task Entrada", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, NULL);
    xTaskCreate(vTaskSaida, "Task Saida", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, NULL);
    xTaskCreate(vTaskReset, "Task Reset", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, NULL);
    xTaskCreate(vTaskBuzzer, "Task Buzzer", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, NULL);
    xTaskCreate(vTaskLEDS, "Task Leds RGB", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, NULL);
    xTaskCreate(vTaskMatrizLeds, "Task Matriz Leds", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, NULL);

    vTaskStartScheduler();
    panic_unsupported();

    while(true){
        sleep_ms(200);
    };
};

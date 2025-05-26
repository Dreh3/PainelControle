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
#include "queue.h"

//Configurações para comunicação I2C
#define I2C_PORT i2c1 //Comucação I2C
#define I2C_SDA 14
#define I2C_SCL 15
#define endereco 0x3C
ssd1306_t ssd; 
bool cor = true;

uint passageiros_ativos = 0;  //Variável global para os ocupantes pq só duas tasks vão poder modificar ela

//Pino do Joystick
#define ButtonJoy 22
//pino botão A
#define ButtonA 5
//Pino do botão
#define ButtonB 6

#define MAX_USUARIOS 20

bool reset = false;
bool limite_atingido = false;
//Criando rótulo da fila
//QueueHandle_t xDadosContagem;

//Criando mutex
SemaphoreHandle_t xMutex;

//Criando semaforos para o eventos de entrada e de saida
SemaphoreHandle_t xSemBinario; //semaforo binário
SemaphoreHandle_t xSemContagemE; //semaforo para o evento de entrada
SemaphoreHandle_t xSemContagemS; //semaforo para o evento de saida

BaseType_t xHigerPriorityTaskWoken = pdFALSE;

//função para mostrar a mensagem padrão
void modelo_Display(){
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
    if(passageiros_ativos==0){
        //espaço vazio
        ssd1306_draw_string(&ssd,"Onibus",37,40);
        ssd1306_draw_string(&ssd,"Vazio!",37,50); 
    }else if(passageiros_ativos>=0 && passageiros_ativos<=MAX_USUARIOS-2){
        //espaço bem frequentado
        ssd1306_draw_string(&ssd,"Poltronas",25,40);
        ssd1306_draw_string(&ssd,"disponiveis!",13,51); 
    }else if(passageiros_ativos==MAX_USUARIOS-1){
        //quase lotado
        ssd1306_draw_string(&ssd,"Ultima poltrona",1,40);
        ssd1306_draw_string(&ssd,"disponivel.",21,51); 
    }else{
        //lotação máxima
        limite_atingido = true;
        ssd1306_draw_string(&ssd,"Lotado!",33,40);
        ssd1306_draw_string(&ssd,"Sem poltronas.",5,51); 
    };
    //printf("contagem atual: %d\n", passageiros_ativos);
};

void vTaskEntrada(){

    while(true){
        if(xSemaphoreTake(xSemContagemE, pdMS_TO_TICKS(50))==pdTRUE){
            if(passageiros_ativos<MAX_USUARIOS){
                passageiros_ativos++;
            };
            //xQueueSend(xDadosContagem, &passageiros_ativos, portMAX_DELAY); //Envia o dado para a fila que vai receber n buzzer e espiar no led
            
            if(xSemaphoreTake(xMutex, portMAX_DELAY)==pdTRUE){

                //chama a função da mensgem padrão
                modelo_Display();

                ssd1306_send_data(&ssd);

                xSemaphoreGive(xMutex);
            }; 
        };
        vTaskDelay(pdMS_TO_TICKS(250));
    };
};


void vTaskSaida(){

    char contagem_atual[3];
    char contagem_maxima[9];    

    while(true){
        if(xSemaphoreTake(xSemContagemS, pdMS_TO_TICKS(50))==pdTRUE){
            if(passageiros_ativos>0){
                passageiros_ativos--;
            };
            //xQueueSend(xDadosContagem, &passageiros_ativos, portMAX_DELAY); //Envia o dado para a fila que vai receber n buzzer e espiar no led
            if(xSemaphoreTake(xMutex, portMAX_DELAY)==pdTRUE){
                
                //chama a função da mensgem padrão
                modelo_Display();

                ssd1306_send_data(&ssd);

                xSemaphoreGive(xMutex);
            }; 
        };
        vTaskDelay(pdMS_TO_TICKS(250));
    };
};


TaskHandle_t xHandleReset;

void vTaskReset(){
    while(true){
        if(xSemaphoreTake(xSemBinario, portMAX_DELAY)==pdTRUE){
            passageiros_ativos = 0;
            reset = true;
            if(xSemaphoreTake(xMutex, portMAX_DELAY)==pdTRUE){
                
                //chama a função da mensgem padrão
                modelo_Display();

                ssd1306_send_data(&ssd);

                xSemaphoreGive(xMutex);
            };
        };
    };
};

uint32_t tempo_anterior = 0;
void InterrupcaoBotao(uint gpio, uint32_t events){
    uint32_t tempo_atual = to_ms_since_boot(get_absolute_time());
    if(tempo_atual - tempo_anterior >= 300){
        tempo_anterior = tempo_atual;
        if(gpio==ButtonJoy){
            xHigerPriorityTaskWoken = pdFALSE;
            xSemaphoreGiveFromISR(xSemBinario, &xHigerPriorityTaskWoken); 
            portYIELD_FROM_ISR(xHigerPriorityTaskWoken);
        }else if(gpio==ButtonA){
            xHigerPriorityTaskWoken = pdFALSE;
            xSemaphoreGiveFromISR(xSemContagemE, &xHigerPriorityTaskWoken); 
            portYIELD_FROM_ISR(xHigerPriorityTaskWoken);
        }else if(gpio==ButtonB){
            xHigerPriorityTaskWoken = pdFALSE;
            xSemaphoreGiveFromISR(xSemContagemS, &xHigerPriorityTaskWoken); 
            portYIELD_FROM_ISR(xHigerPriorityTaskWoken);
        };
    };
};

//Task do buzzer
#define WRAP 65535
#define Buzzer 21
QueueHandle_t xDadosContagem;

void vTaskBuzzer(){

    uint dados_contagem;  //Cria a variável para armazenar os dados da task

    uint sons_freq[] = {880,784,740,659};   //Frequência para os sons de alerta

    //Configurações de PWM
    uint slice;
    gpio_set_function(Buzzer, GPIO_FUNC_PWM);   //Configura pino do led como pwm
    slice = pwm_gpio_to_slice_num(Buzzer);      //Adiquire o slice do pino
    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, clock_get_hz(clk_sys) / (4400 * WRAP));
    pwm_init(slice, &config, true);
    pwm_set_gpio_level(Buzzer, 0);              //Determina com o level desejado - Inicialmente desligado
  
    while(1){

        //Espia o dado na fila e verifica se foi bem sucedido
        //if(xQueueReceive(xDadosContagem, &dados_contagem, portMAX_DELAY) == pdTRUE){
            //Verifica se o dado ultrapassa os limites
            if(reset){
                reset=false;
                for(int i =0; i<4; i++){                //Laço de repetição para modificar as frequência do beep
                    pwm_config_set_clkdiv(&config, clock_get_hz(clk_sys) / (sons_freq[i] * WRAP));
                    pwm_init(slice, &config, true);
                    pwm_set_gpio_level(Buzzer, WRAP/2); //Level metade do wrap para volume médio
                    vTaskDelay(pdMS_TO_TICKS(100));     //pausa entre sons
                };
                pwm_set_gpio_level(Buzzer, 0);          //Desliga o buzzer ao final do ciclo
                vTaskDelay(pdMS_TO_TICKS(100));
            }else if (passageiros_ativos==MAX_USUARIOS && limite_atingido){
                pwm_config_set_clkdiv(&config, clock_get_hz(clk_sys) / (sons_freq[0] * WRAP));
                pwm_init(slice, &config, true);
                pwm_set_gpio_level(Buzzer, WRAP/2);
                vTaskDelay(pdMS_TO_TICKS(100));
                pwm_set_gpio_level(Buzzer, 0);          //Desliga o buzzer, se os dados estiverem no padrão
                vTaskDelay(pdMS_TO_TICKS(100));
                limite_atingido = false;
          //  };
        }else{
            pwm_set_gpio_level(Buzzer, 0);              //Se o dado não for acessado, o buzzer é desligado
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

    //Variável para modificar as luzes da matriz
    uint alerta=0;
    //Padrão para desligar led da matriz
    COR_RGB off = {0.0,0.0,0.0};
    //Vetor para variar entre os estados da matriz
    COR_RGB pessoa_cor = {0.004,0.004,0.001};

    Matriz_leds ocupacao_visual = {{off,off,off,off,off},{off,off,off,off,off},{off,off,off,off,off},{off,off,off,off,off},{off,off,off,off,off}};

    uint controle_contagem = 0;

    acender_leds(ocupacao_visual);

    while(true){

        //for para preencher com os dados certos
        //Matriz com o padrão a ser exibido é atualizada com os valores definidos acima
        controle_contagem = passageiros_ativos;
        for(int i = 0; i <5; i++){
            for(int j=0; j<5;j++){
                if(controle_contagem>0){
                    ocupacao_visual[i][j]=pessoa_cor;
                    controle_contagem --;
                } else if(controle_contagem<=0){
                    ocupacao_visual[i][j]=off;
                }; 
            };
        };
        //Exibe a matriz de leds
        acender_leds(ocupacao_visual);
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

        //if(xQueuePeek(xDadosContagem, &dados_contagem, portMAX_DELAY) == pdTRUE){
            if(passageiros_ativos==0){
                gpio_put(led_Blue,1);
                gpio_put(led_Green,0);
                gpio_put(led_Red,0);
            }else if(passageiros_ativos>=0 && passageiros_ativos<=MAX_USUARIOS-2){
                gpio_put(led_Green,1);
                gpio_put(led_Red,0);
                gpio_put(led_Blue,0);
            }else if(passageiros_ativos==MAX_USUARIOS-1){
                gpio_put(led_Green,1);
                gpio_put(led_Red,1);
                gpio_put(led_Blue,0);
            }else if (passageiros_ativos==MAX_USUARIOS){
               gpio_put(led_Red,1);
               gpio_put(led_Blue,0);
               gpio_put(led_Green,0);
            };
        //};
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
    // Limpa o display. O display inicia com todos os pixels apagados.
    //ssd1306_fill(&ssd, false);
    modelo_Display();
    ssd1306_send_data(&ssd);
    

    //Fila para compartilhar os dados
    xDadosContagem = xQueueCreate (2,sizeof(uint));

    //criar mutexes e semaforos
    xMutex = xSemaphoreCreateMutex();
    xSemBinario = xSemaphoreCreateBinary();
    xSemContagemE = xSemaphoreCreateCounting(MAX_USUARIOS,0);
    xSemContagemS = xSemaphoreCreateCounting(MAX_USUARIOS,0);

    //criando tasks
    xTaskCreate(vTaskEntrada, "Task Entrada", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, NULL);
    xTaskCreate(vTaskSaida, "Task Saida", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, NULL);
    xTaskCreate(vTaskReset, "Task Reset", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, &xHandleReset);
    xTaskCreate(vTaskBuzzer, "Task Buzzer", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, NULL);
    xTaskCreate(vTaskLEDS, "Task Leds RGB", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, NULL);
    xTaskCreate(vTaskMatrizLeds, "Task Matriz Leds", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, NULL);

    vTaskStartScheduler();
    panic_unsupported();

    while(true){
        sleep_ms(200);
    };
};

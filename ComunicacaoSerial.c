/ Declara uma estrutura de fila para a UART
static xQueueHandle qUART = NULL;

// Declara uma estrutura de semaforo para a UART
static xSemaphoreHandle sUART = NULL;

// Declara duas estruturas de mutex para a UART
static xSemaphoreHandle mutexTx = NULL;
static xSemaphoreHandle mutexRx = NULL;

int UART_Init(UBaseType_t size)
{   static int driver_is_installed = false;
    if(driver_is_installed == false){
        sUART = xSemaphoreCreateBinary();
        if (sUART == NULL)
        {
            return DRIVER_FAILURE;
        }else{
            mutexTx = xSemaphoreCreateMutex();
            if (mutexTx == NULL)
            {
                vSemaphoreDelete(sUART);
                return DRIVER_FAILURE;
        }else{
            mutexRx = xSemaphoreCreateMutex();
            if (mutexRx == NULL)
            {
                vSemaphoreDelete(sUART);
                vSemaphoreDelete(mutexTx);
                return DRIVER_FAILURE;
            }else{
                qUART = xQueueCreate(size, sizeof(char));
                if (qUART == NULL)
                {
                    vSemaphoreDelete(sUART);
                    vSemaphoreDelete(mutexTx);
                    vSemaphoreDelete(mutexRx);
                    return DRIVER_FAILURE;
                }else{
                    UART0_Init();
                    driver_is_installed = true;
                    return DRIVER_INITIALIZED;
                }
            }
        }
      }
    }
}

void UART_Send(chat *string, int size){
    // Espera indefinidamente pelo recurso
    if (xSemaphoreTake(mutexTx, portMAX_DELAY) == pdTRUE)
    {   
        while(size){
            // Envia caractere para o buffer da UART
            UART_DATA_R = *string++;

            // Aciona interrupcao de buffer vazio
            UARTIntEnable(INT_TX);

            // Espera indefinidademente por uma interrupcao da UART
            xSemaphoreTake(sUART, portMAX_DELAY);

            // Incrementa o ponteiro de transmissao de dados
            size--;
        }
        // Libera o recurso
        xSemaphoreGive(mutexTx);
    }
}

int UART_Receive(chat *data, int size, int timeout){
    // Espera indefinidamente pelo recurso
    if (xSemaphoreTake(mutexRx, portMAX_DELAY) == pdTRUE)
    {   /* Recurso adquirido, inicia leitura da fila com timeout especificado nos
           parametros da funcao */
        while(size){
            if (xQueueReceive(qUART, data, timeout) == pdTRUE){
                size--;
            }else{
                return DRIVER_TIMEOUT;
            }
        }
        // Libera o recurso
        xSemaphoreGive(mutexRx);
    }
    return DRIVER_SUCCESS;
}

//**************************************************************
// Rotinha de servico de interrupcao da UART
//**************************************************************

void UARTIntHandler(void){
    unsigned int ui32Status;
    portBASE_TYPE pxHigherPrioTaskWokenRX = pdFALSE;
    portBASE_TYPE pxHigherPrioTaskWokenTX = pdFALSE;
    char data;

    // Le flags de interrupcao
    ui32Status = UARTIntStatus();

    // Limpa flags de interrupcao
    UARTIntClear(UART0_BASE, ui32Status);

    // Se for interrupcao de recepcao
    if ((ui32Status&INT_RX) == INT_TX){
        // Recebe o dado do buffer de recepcao
        data = UART_DATA_R;

        // Envia o dado para a fila
        xQueueSendToBackFromISR(qUART, &data, &pxHigherPrioTaskWokenRX);
    }

    // Se for interrupcao de transmissao
    if ((ui32Status&INT_TX) == INT_TX){
        UARTIntDisable(INT_TX);

        /*Informa ao driver que o buffer de transmissao est[a vazio e pronto
        para o proximo caractere*/
        xSemaphoreGiveFromISR(sUART, &pxHigherPrioTaskWokenTX);
    }
    /* Verifica se o semaforo ou a fila acordaram uma tarefa de maior prioridade
    que a tarefa sendo executada atualmanete */
    if ((pxHigherPrioTaskWokenRX == pdTRUE) || (pxHigherPrioTaskWokenTX == pdTRUE)){
        // Se acordou, chama interrupcao de software para troca de contexto
        portyYIELD();
    }
}

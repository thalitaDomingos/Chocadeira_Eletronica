#include <stdio.h>
#include <stdlib.h>

#define FOSC 16000000U // Clock Speed
#define BAUD 9600      // velocidade de comunicao
#define MYUBRR FOSC / 16 / BAUD - 1
#define LED_verde PD7
#define pwmOut (1 << PD6)
#define botao PD2

bool ligado = false;

char msg_tx[20];
char msg_rx[32];

int pos_msg_rx = 0;
int tamanho_msg_rx = 1; // L ou D ou T
int valorResistencia = 0;
float valorTemperatura = 0;

//Prototipos das funcoes
void UART_Init(unsigned int ubrr);
void UART_Transmit(char *dados);


// --------------------------------------------------------- CONFIGURAÇÃO DO ADC ---------------------------------------------------------

void ADC_init(void)
{
  ADMUX = (1 << REFS0);  // Configurando Vref para VCC = 5V
  ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0); // Configuracao do prescaler de 128 para dar 125kHz
}

int ADC_read(u8 ch)
{
  char i;
  int ADC_temp = 0;            // ADC temporário, para manipular leitura
  int ADC_read = 0;            // ADC_read
  ch &= 0x07;

  ADMUX = (ADMUX & 0xF8) | ch; // Zerar os 3 primeiros bits e manter o resto
  ADCSRA |= (1 << ADSC);       // Faça uma conversão


  while (!(ADCSRA & (1 << ADIF)));   // Aguarde a conversão do sinal

  for (i = 0; i < 8; i++)            // Fazendo a conversão 8 vezes para maior precisão
  {
    ADCSRA |= (1 << ADSC);           // Faça uma conversão
    while (!(ADCSRA & (1 << ADIF))); // Aguarde a conversão do sinal
    ADC_temp = ADCL;                 // lê o registro ADCL
    ADC_temp += (ADCH << 8);         // lê o registro ADCH
    ADC_read += ADC_temp;            // Acumula o resultado (8 amostras) para média
  }

  ADC_read = ADC_read >> 3;          // média das 8 amostras ( >> 3 é o mesmo que /8)
  return ADC_read;
}


// ---------------------------------------------------- INTERRUPÇÃO EXTERNA ----------------------------------------------------

ISR(INT0_vect)
{
  if (ligado == true) {
    valorResistencia += 25; // 10% da luminosidade (10% de 255)
    if (valorResistencia > 254)
      valorResistencia = 0;
    OCR0A = valorResistencia;
    UART_Transmit("resistencia = ");
    itoa(valorResistencia, msg_tx, 10);
    UART_Transmit(msg_tx);
    UART_Transmit("\n");
    _delay_ms (100); // para evitar bouncing do botao
  }
}


// --------------------------------------------------------- INT MAIN ---------------------------------------------------------

int main() {

  DDRD |= LED_verde;  // configura saída para o LED VERDE
  DDRD |= pwmOut;     // configura saída para o PWM

  PORTD &= ~(1 << LED_verde); // LED verde inicia desligado
  PORTD &= ~pwmOut;           // PWM inicia desligado
  PORTD |= (1 << botao);      // Pull-up o botao

  u16 adc_result0;
  unsigned long int aux;
  unsigned int tensao;
  ADC_init();         // Inicializa ADC

  EICRA = 0b00000010; // Configura a interrupção para transição de descida
  EIMSK = 0b00000001; // Habilita a interrupção externa
  UART_Init(MYUBRR);  // inicializa a comunicacao serial
  sei();              // Habilita a interrupção global

  // Configura modo FAST PWM e modo do comparador A
  TCCR0A |= (1 << COM0A1);               // Set / Reset
  TCCR0A |= (1 << WGM02) | (1 << WGM00); // phase correct -> fase correta
  TCCR0B = (1 << CS00);                  // Seleciona divisor de clock (1)

  UART_Transmit("Digite 'L' para ligar o sistema \n");
  UART_Transmit("Digite 'D' para desligar o sistema \n");
  UART_Transmit("Digite 'T' para mostrar a temperatura da chocadeira: \n");



  // --------------------------------------------------------- WHILE (1) ---------------------------------------------------------

  while (1)
  {
    // ----------------- VERIFICA O CARACTERE ENVIADO ATRAVES DA UART -----------------

    if (msg_rx[0] == 'L')
    {
      UART_Transmit("LIGADO \n");
      ligado = true;
      msg_rx[0] = ' ';
    }

    if (msg_rx[0] == 'D') {
      UART_Transmit("DESLIGADO \n");
      ligado = false;
      msg_rx[0] = ' ';
    }

    if (msg_rx[0] == 'T')
    {
      UART_Transmit("Temperatura = ");
      itoa(valorTemperatura, msg_tx, 10);
      UART_Transmit(msg_tx);
      UART_Transmit(" °C\n");
      msg_rx[0] = ' ';
    }

    // ----------------- EXECUTA OS COMANDOS DE ACORDO COM O CARACTERE RECEBIDO -----------------

    if (ligado == false)
    {
      PORTD &= ~(1 << LED_verde); // desliga o LED verde
      valorResistencia = 0;
      OCR0A = valorResistencia;

    } else if (ligado == true)
      PORTD |= (1 << LED_verde);  // liga o LED VERDE

    adc_result0 = ADC_read(ADC0D);                     // lê o valor do ADC0 = PC0
    valorTemperatura = (adc_result0 * 0.01956) + 20;   // convertendo 0 a 1023 em 20°C a 40°C
  }
}


// --------------------------------------------------------- CONFIGURAÇÃO DA UART ---------------------------------------------------------

ISR(USART_RX_vect)
{
  msg_rx[pos_msg_rx++] = UDR0;        // Escreve o valor recebido pela UART na posição pos_msg_rx do buffer msg_rx
  if (pos_msg_rx == tamanho_msg_rx)
    pos_msg_rx = 0;
}

void UART_Transmit(char *dados)
{
  while (*dados != 0)                 // Envia todos os caracteres do buffer dados ate chegar um final de linha
  {
    while (!(UCSR0A & (1 << UDRE0))); // Aguarda a transmissão acabar
    UDR0 = *dados;                    // Escreve o caractere no registro de tranmissão
    dados++;                          // Passa para o próximo caractere do buffer dados
  }
}

void UART_Init(unsigned int ubrr)
{
  // Configura a baud rate
  UBRR0H = (unsigned char)(ubrr >> 8);
  UBRR0L = (unsigned char)ubrr;
  UCSR0B = (1 << RXEN0) | (1 << TXEN0) | (1 << RXCIE0);  // Habilita a recepcao, tranmissao e interrupcao na recepcao
  UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);                // Configura o formato da mensagem: 8 bits de dados e 1 bits de stop
}

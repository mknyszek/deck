#define LIN_OUT 1
#define FHT_N 256

#include <FHT.h>

#define F_SAMPLES_N 128 // Samples after squaring
#define F_BAND_N 16     // Frequency subbands
#define F_BAND_SIZE 8   // F_SAMPLES_N/F_BAND_N
#define F_BAND_DIV 3    // -log2(F_BAND_N/F_SAMPLES_N)
#define F_BAND_LIT 3    // Lighting
#define E_HISTORY_N 21  // Past energies
#define E_HISTORY_F 0.0476190 // 1/21

int  buffer[F_BAND_N * E_HISTORY_N]; // Indexed (j + E_HISTORY_N*i)
byte f_on[F_BAND_N]; // array for whether there is beat in a particular subband
byte r, g, b; // rgb output levels

void flash_lights()
{
  int i, j;
  for(i = 0; i < F_BAND_LIT; ++i) 
  {
    if(f_on[i])
    {
      r = 51;
      break;
    }
  }
  
  for(i = F_BAND_LIT; i < F_BAND_LIT*2+1; ++i)
  {
    if(f_on[i])
    {
      g = 51;
      break;
    }
  }
  
  for(i = F_BAND_LIT*2+1; i < F_BAND_LIT*3; ++i)
  {
    if(f_on[i])
    {
      b = 51;
      break;
    }
  }
  
  analogWrite(9,  r);
  analogWrite(10, g);
  analogWrite(11, b);
  
  if(r > 0) r -= 3;
  if(g > 0) g -= 3;
  if(b > 0) b -= 3;
}

void setup() 
{
  TIMSK0 = 0;    // turn off timer0 for lower jitter
  ADCSRA = 0xe5; // set the adc to free running mode
  ADMUX = 0x40;  // use adc0
  DIDR0 = 0x01;  // turn off the digital input for adc0
}

void loop() 
{
  int i, j, hist_size = 0;
  unsigned long sum;   // sum into a long to be safe
  float e_avg, e_inst; // use floats for comparison, not calculation
  
  while(1) // reduces jitter
  { 
    //cli(); // UDRE interrupt slows this way down on arduino1.0
    for (i = 0; i < FHT_N; ++i) // save 256 samples
    { 
      while(!(ADCSRA & 0x10)); // wait for adc to be ready
      ADCSRA = 0xf5; // restart adc
      byte m = ADCL; // fetch adc data
      byte j = ADCH;
      int k = (j << 8) | m; // form into an int
      k -= 0x0200;          // form into a signed int
      k <<= 6;              // form into a 16b signed int
      fht_input[i] = k;     // put real data into bins
    }
    
    fht_window();  // window the data for better frequency response
    fht_reorder(); // reorder the data before doing the fht
    fht_run();     // process the data in the fht
    fht_mag_lin(); // take the output of the fht
    
    // Beat detection algorithm
    for(j = 0; j < F_BAND_N; ++j)
    {
      // Compute instant energy
      sum = 0;
      for(i = 0; i < F_BAND_SIZE; ++i) 
        sum += fht_lin_out[i + j*F_BAND_SIZE] * fht_lin_out[i + j*F_BAND_SIZE];
        
      sum = sum >> F_BAND_DIV;
      buffer[j*E_HISTORY_N] = int(sum);
      
      e_inst = float(sum);
      
      // Compute Average Energy
      if(hist_size == E_HISTORY_N)
      {
        sum = 0;
        for(i = 0; i < hist_size; ++i)
          sum += buffer[i + j*E_HISTORY_N];
        
        e_avg = float(sum) * E_HISTORY_F;
        
        // Compare Energies for Beat in Subband
        if(e_inst > 7.0*e_avg)
          f_on[j] = 1;
        else 
          f_on[j] = 0;
      }
      
      // Shift and Flush Energy History  
      if(hist_size < E_HISTORY_N) 
        i = hist_size-1;
      else
        i = hist_size-2;
        
      for(; i >= 0; --i)
        buffer[1 + i + j*E_HISTORY_N] = buffer[i + j*E_HISTORY_N];
    }
    if(hist_size < E_HISTORY_N) hist_size += 1;
    flash_lights();
  }
}



#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

#include "lookuptable.h"   // lookup_table[], LUT_SIZE, LUT_SCALE, LUT_INDEX_IS_MV

#define MY_ADC_UNIT      ADC_UNIT_1
#define MY_ADC_CHANNEL   ADC_CHANNEL_5          // ADC1_CH5 = GPIO33
#define MY_ADC_ATTEN     ADC_ATTEN_DB_12        // 11 dB está deprecado; usar DB_12 (0–~3.3 V)
#define MY_ADC_BITWIDTH  ADC_BITWIDTH_DEFAULT

// ---------- Calibración ----------
static bool adc_cali_init(adc_unit_t unit, adc_atten_t atten, adc_cali_handle_t *out)
{
    *out = NULL;
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    adc_cali_curve_fitting_config_t cfg = { .unit_id=unit, .atten=atten, .bitwidth=MY_ADC_BITWIDTH };
    if (adc_cali_create_scheme_curve_fitting(&cfg, out) == ESP_OK) return true;
#elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    adc_cali_line_fitting_config_t cfg = { .unit_id=unit, .atten=atten, .bitwidth=MY_ADC_BITWIDTH };
    if (adc_cali_create_scheme_line_fitting(&cfg, out) == ESP_OK) return true;
#endif
    return false;
}

static void adc_cali_deinit(adc_cali_handle_t h)
{
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    if (h) adc_cali_delete_scheme_curve_fitting(h);
#elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    if (h) adc_cali_delete_scheme_line_fitting(h);
#endif
}

// ---------- Tu polinomio: V = f(%), para el benchmark “poly” ----------
static inline double voltage_from_percent(double p)
{
    // f(x) = 4.60897e-09*x^5 - 1.13065e-06*x^4 + 9.13377e-05*x^3
    //        - 0.00277445*x^2 + 0.0639597*x + 0.154238
    double x = p;
    return (((((4.60897e-09*x - 1.13065e-06)*x + 9.13377e-05)*x
              - 2.77445e-03)*x + 6.39597e-02)*x + 1.54238e-01);
}

// Invertir polinomio por bisección: dado mV -> %
static inline double percent_from_mV_poly(int mv)
{
    if (mv < 0) return 0.0;
    double targetV = mv / 1000.0;   // mV -> V
    double lo = 0.0, hi = 100.0;
    for (int i = 0; i < 40; ++i) {
        double mid = 0.5*(lo + hi);
        double Vm  = voltage_from_percent(mid);
        if (Vm < targetV) {
            lo = mid;
        } else {
            hi = mid;
        }
    }
    double p = 0.5*(lo + hi);
    if (p < 0.0) {
        p = 0.0;
    } else if (p > 100.0) {
        p = 100.0;
    }
    return p;
}

// ---------- % por Lookup Table (mV -> %) ----------
static inline double percent_from_lut(int raw, int mv)
{
#if defined(LUT_INDEX_IS_MV) && (LUT_INDEX_IS_MV == 1)
    int idx = mv;                          // índice = mV (0..3300)
#else
    int idx = raw;                         // índice = RAW (0..4095)
#endif
    if (idx < 0) idx = 0;
    if (idx >= LUT_SIZE) idx = LUT_SIZE - 1;

#if (LUT_SCALE == 1)
    return (double)lookup_table[idx];      // % entero
#else
    return lookup_table[idx] / 10.0;       // décimas de %
#endif
}

void app_main(void)
{
    // Unidad ADC
    adc_oneshot_unit_handle_t adc;
    adc_oneshot_unit_init_cfg_t ucfg = { .unit_id = MY_ADC_UNIT };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&ucfg, &adc));

    // Canal
    adc_oneshot_chan_cfg_t ccfg = { .bitwidth = MY_ADC_BITWIDTH, .atten = MY_ADC_ATTEN };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc, MY_ADC_CHANNEL, &ccfg));

    // Calibración -> mV
    adc_cali_handle_t cali = NULL;
    bool do_cali = adc_cali_init(MY_ADC_UNIT, MY_ADC_ATTEN, &cali);

    while (1) {
        int raw = 0, mv = -1;
        ESP_ERROR_CHECK(adc_oneshot_read(adc, MY_ADC_CHANNEL, &raw));
#if defined(LUT_INDEX_IS_MV) && (LUT_INDEX_IS_MV == 1)
        if (do_cali) {
            if (adc_cali_raw_to_voltage(cali, raw, &mv) != ESP_OK) mv = -1;
        } else {
            mv = (int)((raw / 4095.0) * 3300.0 + 0.5);
        }
#endif

        // ---- Benchmark: polinomio ----
        int64_t t0 = esp_timer_get_time();
        double pct_poly = percent_from_mV_poly(mv);
        int64_t t1 = esp_timer_get_time();

        // ---- Benchmark: LUT ----
        int64_t t2 = esp_timer_get_time();
        double pct_lut  = percent_from_lut(raw, mv);
        int64_t t3 = esp_timer_get_time();

        // Resultado + tiempos (µs)
        printf("raw=%4d mv=%4d | poly=%.1f%% (%lld us) | LUT=%.1f%% (%lld us)\n",
               raw, mv, pct_poly, (long long)(t1 - t0), pct_lut, (long long)(t3 - t2));

        vTaskDelay(pdMS_TO_TICKS(200));
    }

    adc_cali_deinit(cali);
    ESP_ERROR_CHECK(adc_oneshot_del_unit(adc));
}


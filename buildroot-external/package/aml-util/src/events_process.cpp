#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <linux/input.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <ctype.h>
#include "events.h"
#include "events_process.h"

#define WAIT_KEY_TIMEOUT_SEC    120
#define nullptr NULL
#define KEY_EVENT_TIME_INTERVAL 20

#define DBG(format, arg...) printf(__FILE__ ": %s(), L%d: " format "\n", __func__, __LINE__, ## arg)

EventsProcess:: KeyMapItem_t g_default_keymap[] = {
    { "power", KEY_POWER },
    { "down", KEY_DOWN },
    { "up", KEY_UP },
};

EventsProcess::EventsProcess()
        : key_queue_len(0),
          key_last_down(-1),
          last_key(-1),
          key_down_count(0),
          report_longpress_flag(false),
          num_keys(0),
          keys_map(NULL) {
    pthread_mutex_init(&key_queue_mutex, nullptr);
    pthread_cond_init(&key_queue_cond, nullptr);
    memset(key_pressed, 0, sizeof(key_pressed));
    memset(&last_queue_time, 0, sizeof(last_queue_time));
    load_key_map();
}


int EventsProcess::InputCallback(int fd, uint32_t epevents, void* data) {
    return reinterpret_cast<EventsProcess*>(data)->OnInputEvent(fd, epevents);
}

static void* InputThreadLoop(void*) {
    while (true) {
        if (!ev_wait(-1)) {
            ev_dispatch();
        }
    }
    return nullptr;
}

void EventsProcess::Init() {
    ev_init(InputCallback, this);

    pthread_create(&input_thread_, nullptr, InputThreadLoop, nullptr);
}

int EventsProcess::OnInputEvent(int fd, uint32_t epevents) {
    struct input_event ev;

    if (ev_get_input(fd, epevents, &ev) == -1) {
        return -1;
    }

    if (ev.type == EV_SYN) {
        return 0;
    }

    if (ev.type == EV_KEY && ev.code <= KEY_MAX) {

        int code = getMapKey(ev.code);
        if (code > 0) {
            ProcessKey(code, ev.value);
        } else {
            ProcessKey(ev.code, ev.value);
        }
    }

    return 0;
}

/* use CLOCK_MONOTONIC clock, gettimeofday will overlap if ntp changes */
uint64_t EventsProcess::getSystemTimeUs() {
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (ts.tv_sec)*1000000LL + ts.tv_nsec/1000LL;
}

void EventsProcess::ProcessKey(int key_code, int value) {
    bool register_key = false;
    bool flag = false;
    pthread_mutex_lock(&key_queue_mutex);
    key_pressed[key_code] = value;
    if (value == 1) {/*1:key down*/
        KeyEvent *ke = new KeyEvent;
        ke->down_ts_us = getSystemTimeUs();
        ke->keyCode = key_code;
        ke->keyState = value; /*key down*/
        ke->longPress = false;
        ke->curlongPress = false;
        mKeyEventVec.push_back(ke);

        ++key_down_count;
        key_last_down = key_code;
        key_timer_t* info = new key_timer_t;
        info->ep = this;
        info->key_code = key_code;
        info->count = key_down_count;
        info->ke = ke;
        pthread_t thread;
        pthread_create(&thread, nullptr, &EventsProcess::time_key_helper, info);
        pthread_detach(thread);
    } else if(value == 2){/*2:key repeat*/
    } else {/*0:key down*/
        for (std::vector<KeyEvent *>::iterator iter = mKeyEventVec.begin();
                iter != mKeyEventVec.end(); ++iter) {
            if ((*iter)->keyCode == key_code && (*iter)->keyState == 1) {
                (*iter)->up_ts_us = getSystemTimeUs();
                (*iter)->keyState = value; /*key up*/
                if ((*iter)->longPress || (*iter)->curlongPress) {
                    (*iter)->curlongPress = false;
                    flag = true;
                }
                break;
            }
        }

        if (key_last_down == key_code) {
            register_key = true;
        }
        key_last_down = -1;
    }
    pthread_mutex_unlock(&key_queue_mutex);
    last_key = key_code;
    if (register_key) {
        EnqueueKey(key_code, flag);
    }
}

void* EventsProcess::time_key_helper(void* cookie) {
    key_timer_t* info = (key_timer_t*) cookie;
    info->ep->time_key(info);
    delete info;
    return nullptr;
}

// longpress = 15s
const int LONG_PRESS_SECONDS = 15;
void EventsProcess::time_key(key_timer_t *info) {
    int key_code = info->key_code;
    int count = info->count;
    int keymode = getKeyMode(key_code);
    int second_count = 0;

    while (second_count < LONG_PRESS_SECONDS) {
        sleep(1);
        if (key_last_down < 0) {
            // release the button less than 15 seconds
            DBG("button released less than %d seconds. second_count: %d", LONG_PRESS_SECONDS, second_count);
            report_longpress_flag = false;
            return;
        }
        second_count++;
    }

    pthread_mutex_lock(&key_queue_mutex);
    if (key_last_down == key_code && key_down_count == count) {
        if (info->ke) {
            info->ke->longPress = true;
            if (keymode != 0)
                info->ke->curlongPress = true;
                system("/usr/sbin/led.sh set_led 255 255 0");
        }
    }
    if (info->ke->longPress && info->ke->curlongPress) {
        pthread_mutex_unlock(&key_queue_mutex);
        while (key_last_down == key_code && key_down_count == count) {
            EnqueueKey(key_code, false);
        }
    } else {
        pthread_mutex_unlock(&key_queue_mutex);
    }
}


const char* EventsProcess::getKeyType(int key) {
    int i;
    for (i = 0; i < num_keys; i++) {
        KeyMapItem_t* v = &keys_map[i];
        if (v->value == key)
            return v->type;
    }

    return NULL;
}

int EventsProcess::getKeyMode(int key) {
    int i;
    for (i = 0; i < num_keys; i++) {
        KeyMapItem_t* v = &keys_map[i];
        if (v->value == key)
            return v->mode;
    }

    return 0;
}

void EventsProcess::load_key_map() {
    FILE* fstab = fopen("/etc/gpio_key.kl", "r");
    if (fstab != NULL) {
        printf("loaded /etc/gpio_key.kl\n");
        int alloc = 2;
        keys_map = (KeyMapItem_t*)malloc(alloc * sizeof(KeyMapItem_t));
        num_keys = 0;

        char buffer[1024];
        int i;
        int value = -1;
        while (fgets(buffer, sizeof(buffer)-1, fstab)) {
            for (i = 0; buffer[i] && isspace(buffer[i]); ++i);

            if (buffer[i] == '\0' || buffer[i] == '#') continue;

            char* original = strdup(buffer);
            char* type = strtok(original+i, " \t\n");
            char* key = strtok(NULL, " \t\n");
            char* mode = strtok(NULL, " \t\n");
            value = atoi (key);
            if (type && key && (value > 0)) {
                while (num_keys >= alloc) {
                    alloc *= 2;
                    keys_map = (KeyMapItem_t*)realloc(keys_map, alloc*sizeof(KeyMapItem_t));
                }
                keys_map[num_keys].type = strdup(type);
                keys_map[num_keys].value = value;
                if (!mode)
                    keys_map[num_keys].mode = 0;
                else
                    keys_map[num_keys].mode = atoi (mode);

                ++num_keys;
            } else {
                printf("error: skipping malformed keyboard.lk line: %s\n", original);
            }
            free(original);
        }

        fclose(fstab);
    } else {
        printf("error: failed to open /etc/gpio_key.kl, use default map\n");
        num_keys = DEFAULT_KEY_NUM;
        keys_map = g_default_keymap;
    }

    printf("keyboard key map table:\n");
    int i;
    for (i = 0; i < num_keys; ++i) {
        KeyMapItem_t* v = &keys_map[i];
        printf("  %d type:%s value:%d mode:%d\n", i, v->type, v->value, v->mode);
    }
}

int EventsProcess::getMapKey(int key) {
    int i;
    for (i = 0; i < num_keys; i++) {
        KeyMapItem_t* v = &keys_map[i];
        if (v->value == key)
            return v->value;
    }

    return -1;
}

long checkEventTime(struct timeval *before, struct timeval *later) {
    time_t before_sec = before->tv_sec;
    suseconds_t before_usec = before->tv_usec;
    time_t later_sec = later->tv_sec;
    suseconds_t later_usec = later->tv_usec;

    long sec_diff = (later_sec - before_sec) * 1000;
    if (sec_diff < 0)
        return true;

    long ret = sec_diff + (later_usec - before_usec) / 1000;
    if (ret >= KEY_EVENT_TIME_INTERVAL)
        return true;

    return false;
}

void EventsProcess::EnqueueKey(int key_code, bool flag) {
    struct timeval now;
    gettimeofday(&now, nullptr);

    pthread_mutex_lock(&key_queue_mutex);
    const int queue_max = sizeof(key_queue) / sizeof(key_queue[0]);
    if (key_queue_len < queue_max) {
        if (last_key != key_code || checkEventTime(&last_queue_time, &now) || flag) {
            key_queue[key_queue_len++] = key_code;
            last_queue_time = now;
        }
        pthread_cond_signal(&key_queue_cond);
    }
    pthread_mutex_unlock(&key_queue_mutex);
}

#ifdef FILTER_POWERKEY
bool filter_power_key(const char* key_type)
{
#define FILTER_KEY_TIME 150   //ms
    static long last_time_ms = -1;

    if (!strcmp(key_type, "power")) {
        struct timeval now_time;
        gettimeofday(&now_time, nullptr);
        long now_time_ms = now_time.tv_sec*1000 + now_time.tv_usec/1000;
        if (last_time_ms == -1 || (now_time_ms - last_time_ms) >= FILTER_KEY_TIME) {
            last_time_ms = now_time_ms;
            return true;
        } else {
            return false;
        }
    } else {
        return true;
    }
#undef FILTER_KEY_TIME
}
#endif

void EventsProcess::WaitKey() {
    bool cur_longpress = false;
    bool execute = false;
    pthread_mutex_lock(&key_queue_mutex);

    do {
        struct timeval now;
        struct timespec timeout;
        gettimeofday(&now, nullptr);
        timeout.tv_sec = now.tv_sec;
        timeout.tv_nsec = now.tv_usec * 1000;
        timeout.tv_sec += WAIT_KEY_TIMEOUT_SEC;

        int rc = 0;
        while (key_queue_len == 0 && rc != ETIMEDOUT) {
            rc = pthread_cond_timedwait(&key_queue_cond, &key_queue_mutex, &timeout);
        }
    } while (key_queue_len == 0);

    int key = -1;
    KeyEvent *keyEvent = nullptr;
    char* event_str;
    char buf[100];
    if (key_queue_len > 0) {
        key = key_queue[0];
        memcpy(&key_queue[0], &key_queue[1], sizeof(int) * --key_queue_len);
    }

    std::vector<KeyEvent *>::iterator iter = mKeyEventVec.begin();
    //should remove zombie key down long long time?
    for (; iter != mKeyEventVec.end();) {
        if ((*iter)->keyCode == key && ((*iter)->keyState == 0 || (*iter)->curlongPress)) {
            keyEvent = *iter;
            if (keyEvent->curlongPress)
                cur_longpress = true;
            else
                iter = mKeyEventVec.erase(iter);
            break;
        } else {
            ++iter;
        }
    }

    pthread_mutex_unlock(&key_queue_mutex);
    const char* keyType=getKeyType(key);
    int keymode=getKeyMode(key);
    int res;
    memset(buf,'\0',sizeof(buf));
    if (keyType != NULL) {
        if (keyEvent && cur_longpress && keyEvent->longPress && keymode != 0) {
            sprintf(buf,"%s %s%s","/etc/adckey/adckey_function.sh","curlongpress",keyType);
            execute = true;
        } else if (keyEvent && !cur_longpress && keyEvent->longPress && keymode != 1) {
            uint64_t duration = 0;
            if (keyEvent->up_ts_us < keyEvent->down_ts_us) {
                duration = UINT64_MAX - keyEvent->down_ts_us + keyEvent->up_ts_us;
                duration /= 1000LL;
            } else {
                duration = (keyEvent->up_ts_us - keyEvent->down_ts_us)/1000LL;
            }

            sprintf(buf,"%s %s%s %llu","/etc/adckey/adckey_function.sh","longpress",keyType, duration);
            execute = true;
        } else if (!(keyEvent && keyEvent->longPress)) {
            sprintf(buf,"%s %s","/etc/adckey/adckey_function.sh",keyType);
#ifdef FILTER_POWERKEY
            execute = filter_power_key(keyType);
#else
            execute = true;
#endif
        }
        if (execute) {
            //printf("input_eventd: run %s\n", buf);
            res=system(buf);
            if (res != 0) printf("run %s exception!!!\n",buf);
        }
    }

    //release
    if (!cur_longpress)
        delete keyEvent;
}

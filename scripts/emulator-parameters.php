<?php

if (!function_exists('phuamqp_env')) {
    function phuamqp_env(string $name, string $default): string
    {
        $value = getenv($name);
        return ($value === false || $value === '') ? $default : $value;
    }
}

if (!function_exists('phuamqp_env_bool')) {
    function phuamqp_env_bool(string $name, bool $default): bool
    {
        $value = getenv($name);
        if ($value === false || $value === '') {
            return $default;
        }

        return filter_var($value, FILTER_VALIDATE_BOOLEAN, FILTER_NULL_ON_FAILURE) ?? $default;
    }
}

define('PHUAMQP_HOST', phuamqp_env('PHUAMQP_HOST', 'phuamqp-servicebus-emulator'));
define('PHUAMQP_PORT', (int) phuamqp_env('PHUAMQP_PORT', '5672'));
define('PHUAMQP_USE_TLS', phuamqp_env_bool('PHUAMQP_USE_TLS', false));
define('PHUAMQP_KEY_NAME', phuamqp_env('PHUAMQP_KEY_NAME', 'RootManageSharedAccessKey'));
define('PHUAMQP_ACCESS_KEY', phuamqp_env('PHUAMQP_ACCESS_KEY', 'EmulatorPassword123!'));
define('PHUAMQP_QUEUE_NAME', phuamqp_env('PHUAMQP_QUEUE_NAME', 'test-queue'));
define('PHUAMQP_MESSAGE_COUNT', (int) phuamqp_env('PHUAMQP_MESSAGE_COUNT', '3'));
define('PHUAMQP_CONSUMER_TIMEOUT', (int) phuamqp_env('PHUAMQP_CONSUMER_TIMEOUT', '60'));
define('PHUAMQP_STARTUP_WAIT', (int) phuamqp_env('PHUAMQP_STARTUP_WAIT', '3'));


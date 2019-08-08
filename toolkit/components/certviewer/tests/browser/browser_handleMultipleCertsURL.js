// valids
const multipleCerts1 =
  "about:certificate?cert=MIIGRjCCBS6gAwIBAgIQDJduPkI49CDWPd%2BG7%2Bu6kDANBgkqhkiG9w0BAQsFADBNMQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMScwJQYDVQQDEx5EaWdpQ2VydCBTSEEyIFNlY3VyZSBTZXJ2ZXIgQ0EwHhcNMTgxMTA1MDAwMDAwWhcNMTkxMTEzMTIwMDAwWjCBgzELMAkGA1UEBhMCVVMxEzARBgNVBAgTCkNhbGlmb3JuaWExFjAUBgNVBAcTDU1vdW50YWluIFZpZXcxHDAaBgNVBAoTE01vemlsbGEgQ29ycG9yYXRpb24xDzANBgNVBAsTBldlYk9wczEYMBYGA1UEAxMPd3d3Lm1vemlsbGEub3JnMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAuKruymkkmkqCJh7QjmXlUOBcLFRyw5LG%2FvUUWVrsxC2gsbR8WJq%2BcYoYBpoNVStKrO4U2rBh1GEbccvT6qKOQI%2BpjjDxx9cmRdubGTGp8L0MF1ohVvhIvYLumOEoRDDPU4PvGJjGhek%2FojvedPWe8dhciHkxOC2qPFZvVFMwg1%2Fo%2Fb80147BwZQmzB18mnHsmcyKlpsCN8pxw86uao9Iun8gZQrsllW64rTZlRR56pHdAcuGAoZjYZxwS9Z%2BlvrSjEgrddemWyGGalqyFp1rXlVM1Tf4%2FIYWAQXTgTUN303u3xMjss7QK7eUDsACRxiWPLW9XQDd1c%2ByvaYJKzgJ2wIDAQABo4IC6TCCAuUwHwYDVR0jBBgwFoAUD4BhHIIxYdUvKOeNRji0LOHG2eIwHQYDVR0OBBYEFNpSvSGcN2VT%2FB9TdQ8eXwebo60%2FMCcGA1UdEQQgMB6CD3d3dy5tb3ppbGxhLm9yZ4ILbW96aWxsYS5vcmcwDgYDVR0PAQH%2FBAQDAgWgMB0GA1UdJQQWMBQGCCsGAQUFBwMBBggrBgEFBQcDAjBrBgNVHR8EZDBiMC%2BgLaArhilodHRwOi8vY3JsMy5kaWdpY2VydC5jb20vc3NjYS1zaGEyLWc2LmNybDAvoC2gK4YpaHR0cDovL2NybDQuZGlnaWNlcnQuY29tL3NzY2Etc2hhMi1nNi5jcmwwTAYDVR0gBEUwQzA3BglghkgBhv1sAQEwKjAoBggrBgEFBQcCARYcaHR0cHM6Ly93d3cuZGlnaWNlcnQuY29tL0NQUzAIBgZngQwBAgIwfAYIKwYBBQUHAQEEcDBuMCQGCCsGAQUFBzABhhhodHRwOi8vb2NzcC5kaWdpY2VydC5jb20wRgYIKwYBBQUHMAKGOmh0dHA6Ly9jYWNlcnRzLmRpZ2ljZXJ0LmNvbS9EaWdpQ2VydFNIQTJTZWN1cmVTZXJ2ZXJDQS5jcnQwDAYDVR0TAQH%2FBAIwADCCAQIGCisGAQQB1nkCBAIEgfMEgfAA7gB1AKS5CZC0GFgUh7sTosxncAo8NZgE%2BRvfuON3zQ7IDdwQAAABZuYWiHwAAAQDAEYwRAIgZnMSH1JdG6NASHWTwD0mlP%2Fzbr0hzP263c02Ym0DU64CIEe4QHJDP47j0b6oTFu6RrZz1NQ9cq8Az1KnMKRuaFAlAHUAh3W%2F51l8%2BIxDmV%2B9827%2FVo1HVjb%2FSrVgwbTq%2F16ggw8AAAFm5haJAgAABAMARjBEAiAxGLXkUaOAkZhXNeNR3pWyahZeKmSaMXadgu18SfK1ZAIgKtwu5eGxK76rgaszLCZ9edBIjuU0DKorzPUuxUXFY0QwDQYJKoZIhvcNAQELBQADggEBAKLJAFO3wuaP5MM%2Fed1lhk5Uc2aDokhcM7XyvdhEKSHbgPhcgMoT9YIVoPa70gNC6KHcwoXu0g8wt7X6Vm1ql%2F68G5q844kFuC6JPl4LVT9mciD%2BVW6bHUSXD9xifL9DqdJ0Ic0SllTlM%2Boq5aAeOxUQGXhXIqj6fSQv9fQN6mXxQIoc%2Fgjxteskq%2FVl8YmY1FIZP9Bh7g27kxZ9GAAGQtjTL03RzKAuSg6yeImYVdQWasc7UPnBXlRAzZ8%2BOJThUbzK16a2CI3Rg4agKSJk%2BuA47h1%2FImmngpFLRb%2FMvRX6H1oWcUuyH6O7PZdl0YpwTpw1THIuqCGl%2FwpPgyQgcTM%3D&cert=MIIHQjCCBiqgAwIBAgIQCgYwQn9bvO1pVzllk7ZFHzANBgkqhkiG9w0BAQsFADB1MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3d3cuZGlnaWNlcnQuY29tMTQwMgYDVQQDEytEaWdpQ2VydCBTSEEyIEV4dGVuZGVkIFZhbGlkYXRpb24gU2VydmVyIENBMB4XDTE4MDUwODAwMDAwMFoXDTIwMDYwMzEyMDAwMFowgccxHTAbBgNVBA8MFFByaXZhdGUgT3JnYW5pemF0aW9uMRMwEQYLKwYBBAGCNzwCAQMTAlVTMRkwFwYLKwYBBAGCNzwCAQITCERlbGF3YXJlMRAwDgYDVQQFEwc1MTU3NTUwMQswCQYDVQQGEwJVUzETMBEGA1UECBMKQ2FsaWZvcm5pYTEWMBQGA1UEBxMNU2FuIEZyYW5jaXNjbzEVMBMGA1UEChMMR2l0SHViLCBJbmMuMRMwEQYDVQQDEwpnaXRodWIuY29tMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAxjyq8jyXDDrBTyitcnB90865tWBzpHSbindG%2FXqYQkzFMBlXmqkzC%2BFdTRBYyneZw5Pz%2BXWQvL%2B74JW6LsWNc2EF0xCEqLOJuC9zjPAqbr7uroNLghGxYf13YdqbG5oj%2F4x%2BogEG3dF%2FU5YIwVr658DKyESMV6eoYV9mDVfTuJastkqcwero%2B5ZAKfYVMLUEsMwFtoTDJFmVf6JlkOWwsxp1WcQ%2FMRQK1cyqOoUFUgYylgdh3yeCDPeF22Ax8AlQxbcaI%2BGwfQL1FB7Jy%2Bh%2BKjME9lE%2FUpgV6Qt2R1xNSmvFCBWu%2BNFX6epwFP%2FJRbkMfLz0beYFUvmMgLtwVpEPSwIDAQABo4IDeTCCA3UwHwYDVR0jBBgwFoAUPdNQpdagre7zSmAKZdMh1Pj41g8wHQYDVR0OBBYEFMnCU2FmnV%2BrJfQmzQ84mqhJ6kipMCUGA1UdEQQeMByCCmdpdGh1Yi5jb22CDnd3dy5naXRodWIuY29tMA4GA1UdDwEB%2FwQEAwIFoDAdBgNVHSUEFjAUBggrBgEFBQcDAQYIKwYBBQUHAwIwdQYDVR0fBG4wbDA0oDKgMIYuaHR0cDovL2NybDMuZGlnaWNlcnQuY29tL3NoYTItZXYtc2VydmVyLWcyLmNybDA0oDKgMIYuaHR0cDovL2NybDQuZGlnaWNlcnQuY29tL3NoYTItZXYtc2VydmVyLWcyLmNybDBLBgNVHSAERDBCMDcGCWCGSAGG%2FWwCATAqMCgGCCsGAQUFBwIBFhxodHRwczovL3d3dy5kaWdpY2VydC5jb20vQ1BTMAcGBWeBDAEBMIGIBggrBgEFBQcBAQR8MHowJAYIKwYBBQUHMAGGGGh0dHA6Ly9vY3NwLmRpZ2ljZXJ0LmNvbTBSBggrBgEFBQcwAoZGaHR0cDovL2NhY2VydHMuZGlnaWNlcnQuY29tL0RpZ2lDZXJ0U0hBMkV4dGVuZGVkVmFsaWRhdGlvblNlcnZlckNBLmNydDAMBgNVHRMBAf8EAjAAMIIBfgYKKwYBBAHWeQIEAgSCAW4EggFqAWgAdgCkuQmQtBhYFIe7E6LMZ3AKPDWYBPkb37jjd80OyA3cEAAAAWNBYm0KAAAEAwBHMEUCIQDRZp38cTWsWH2GdBpe%2FuPTWnsu%2Fm4BEC2%2BdIcvSykZYgIgCP5gGv6yzaazxBK2NwGdmmyuEFNSg2pARbMJlUFgU5UAdgBWFAaaL9fC7NP14b1Esj7HRna5vJkRXMDvlJhV1onQ3QAAAWNBYm0tAAAEAwBHMEUCIQCi7omUvYLm0b2LobtEeRAYnlIo7n6JxbYdrtYdmPUWJQIgVgw1AZ51vK9ENinBg22FPxb82TvNDO05T17hxXRC2IYAdgC72d%2B8H4pxtZOUI5eqkntHOFeVCqtS6BqQlmQ2jh7RhQAAAWNBYm3fAAAEAwBHMEUCIQChzdTKUU2N%2BXcqcK0OJYrN8EYynloVxho4yPk6Dq3EPgIgdNH5u8rC3UcslQV4B9o0a0w204omDREGKTVuEpxGeOQwDQYJKoZIhvcNAQELBQADggEBAHAPWpanWOW%2Fip2oJ5grAH8mqQfaunuCVE%2Bvac%2B88lkDK%2FLVdFgl2B6kIHZiYClzKtfczG93hWvKbST4NRNHP9LiaQqdNC17e5vNHnXVUGw%2ByxyjMLGqkgepOnZ2Rb14kcTOGp4i5AuJuuaMwXmCo7jUwPwfLe1NUlVBKqg6LK0Hcq4K0sZnxE8HFxiZ92WpV2AVWjRMEc%2F2z2shNoDvxvFUYyY1Oe67xINkmyQKc%2BygSBZzyLnXSFVWmHr3u5dcaaQGGAR42v6Ydr4iL38Hd4dOiBma%2BFXsXBIqWUjbST4VXmdaol7uzFMojA4zkxQDZAvF5XgJlAFadfySna%2Fteik%3D";
const multipleCerts2 =
  "about:certificate?cert=MIIIIDCCBwigAwIBAgIQGk0sGNQUuaOL9Rii2XQ4yjANBgkqhkiG9w0BAQsFADBUMQswCQYDVQQGEwJVUzEeMBwGA1UEChMVR29vZ2xlIFRydXN0IFNlcnZpY2VzMSUwIwYDVQQDExxHb29nbGUgSW50ZXJuZXQgQXV0aG9yaXR5IEczMB4XDTE5MDYxODA4MjE1OFoXDTE5MDkxMDA4MTUwMFowZjELMAkGA1UEBhMCVVMxEzARBgNVBAgMCkNhbGlmb3JuaWExFjAUBgNVBAcMDU1vdW50YWluIFZpZXcxEzARBgNVBAoMCkdvb2dsZSBMTEMxFTATBgNVBAMMDCouZ29vZ2xlLmNvbTBZMBMGByqGSM49AgEGCCqGSM49AwEHA0IABMRScn8kk6qy3LHVktWZWxm%2FMq4kowlEdxQH40wijThZ%2B%2F5Jrqh6UlWnWuiulNorHH2DEW4OkSKreFoYze7w8O6jggWlMIIFoTATBgNVHSUEDDAKBggrBgEFBQcDATAOBgNVHQ8BAf8EBAMCB4AwggRqBgNVHREEggRhMIIEXYIMKi5nb29nbGUuY29tgg0qLmFuZHJvaWQuY29tghYqLmFwcGVuZ2luZS5nb29nbGUuY29tghIqLmNsb3VkLmdvb2dsZS5jb22CGCouY3Jvd2Rzb3VyY2UuZ29vZ2xlLmNvbYIGKi5nLmNvgg4qLmdjcC5ndnQyLmNvbYIRKi5nY3BjZG4uZ3Z0MS5jb22CCiouZ2dwaHQuY26CFiouZ29vZ2xlLWFuYWx5dGljcy5jb22CCyouZ29vZ2xlLmNhggsqLmdvb2dsZS5jbIIOKi5nb29nbGUuY28uaW6CDiouZ29vZ2xlLmNvLmpwgg4qLmdvb2dsZS5jby51a4IPKi5nb29nbGUuY29tLmFygg8qLmdvb2dsZS5jb20uYXWCDyouZ29vZ2xlLmNvbS5icoIPKi5nb29nbGUuY29tLmNvgg8qLmdvb2dsZS5jb20ubXiCDyouZ29vZ2xlLmNvbS50coIPKi5nb29nbGUuY29tLnZuggsqLmdvb2dsZS5kZYILKi5nb29nbGUuZXOCCyouZ29vZ2xlLmZyggsqLmdvb2dsZS5odYILKi5nb29nbGUuaXSCCyouZ29vZ2xlLm5sggsqLmdvb2dsZS5wbIILKi5nb29nbGUucHSCEiouZ29vZ2xlYWRhcGlzLmNvbYIPKi5nb29nbGVhcGlzLmNughEqLmdvb2dsZWNuYXBwcy5jboIUKi5nb29nbGVjb21tZXJjZS5jb22CESouZ29vZ2xldmlkZW8uY29tggwqLmdzdGF0aWMuY26CDSouZ3N0YXRpYy5jb22CEiouZ3N0YXRpY2NuYXBwcy5jboIKKi5ndnQxLmNvbYIKKi5ndnQyLmNvbYIUKi5tZXRyaWMuZ3N0YXRpYy5jb22CDCoudXJjaGluLmNvbYIQKi51cmwuZ29vZ2xlLmNvbYIWKi55b3V0dWJlLW5vY29va2llLmNvbYINKi55b3V0dWJlLmNvbYIWKi55b3V0dWJlZWR1Y2F0aW9uLmNvbYIRKi55b3V0dWJla2lkcy5jb22CByoueXQuYmWCCyoueXRpbWcuY29tghphbmRyb2lkLmNsaWVudHMuZ29vZ2xlLmNvbYILYW5kcm9pZC5jb22CG2RldmVsb3Blci5hbmRyb2lkLmdvb2dsZS5jboIcZGV2ZWxvcGVycy5hbmRyb2lkLmdvb2dsZS5jboIEZy5jb4IIZ2dwaHQuY26CBmdvby5nbIIUZ29vZ2xlLWFuYWx5dGljcy5jb22CCmdvb2dsZS5jb22CD2dvb2dsZWNuYXBwcy5jboISZ29vZ2xlY29tbWVyY2UuY29tghhzb3VyY2UuYW5kcm9pZC5nb29nbGUuY26CCnVyY2hpbi5jb22CCnd3dy5nb28uZ2yCCHlvdXR1LmJlggt5b3V0dWJlLmNvbYIUeW91dHViZWVkdWNhdGlvbi5jb22CD3lvdXR1YmVraWRzLmNvbYIFeXQuYmUwaAYIKwYBBQUHAQEEXDBaMC0GCCsGAQUFBzAChiFodHRwOi8vcGtpLmdvb2cvZ3NyMi9HVFNHSUFHMy5jcnQwKQYIKwYBBQUHMAGGHWh0dHA6Ly9vY3NwLnBraS5nb29nL0dUU0dJQUczMB0GA1UdDgQWBBT8E5DiGEc7xHswBMF5K5EmFzx6fDAMBgNVHRMBAf8EAjAAMB8GA1UdIwQYMBaAFHfCuFCaZ3Z2sS3ChtCDoH6mfrpLMCEGA1UdIAQaMBgwDAYKKwYBBAHWeQIFAzAIBgZngQwBAgIwMQYDVR0fBCowKDAmoCSgIoYgaHR0cDovL2NybC5wa2kuZ29vZy9HVFNHSUFHMy5jcmwwDQYJKoZIhvcNAQELBQADggEBAGrWCimTdP8%2B6qCqNh1Xza9Wlm%2FGDVb%2BKp0PHUZaNDMThFQQqGn2XVi7ZkhqsxTtMw88Nrl97Bi1OsnNPlabhDwxtRyWkNkLdpFxebRz73CsjyoB3cboH%2FZ8sU9LLwXe%2FIJgMkvT7fznKtKSweWV%2BmAYG%2F3KzaIyLrowYpyOsWaYytPJK7ypuk6yOPxuQcQnelotWpuHUBR3mo3FIW56BPCy20KwqwvWoZe27BNxO%2F11Ym%2FPnric2fWXwgmHdkvVqqssUMFs%2BmG2tS%2FpqnR5yZdQL60xOgDq70mVWYgQT4yUTm7i8qvugvNnuSIjjsbelHgbC8f5%2Fx%2Bo2pEyw%2FDYyKw%3D&cert=MIIGcTCCBVmgAwIBAgIQCHx760AsIFydVjey10GPXTANBgkqhkiG9w0BAQsFADBwMQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3d3cuZGlnaWNlcnQuY29tMS8wLQYDVQQDEyZEaWdpQ2VydCBTSEEyIEhpZ2ggQXNzdXJhbmNlIFNlcnZlciBDQTAeFw0xOTA1MjQwMDAwMDBaFw0yMDA1MjMxMjAwMDBaMHkxCzAJBgNVBAYTAlVTMRMwEQYDVQQIEwpDYWxpZm9ybmlhMRYwFAYDVQQHEw1TYW4gRnJhbmNpc2NvMRYwFAYDVQQKEw1Ud2l0dGVyLCBJbmMuMQ0wCwYDVQQLEwRhdGxhMRYwFAYDVQQDDA0qLnR3aXR0ZXIuY29tMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEA4IL8Mo0RIfgZmVhwMiaq30EfjJzcB1dalwHZb2K6P6LtmEpLs%2Bnqm1FCdfZgKO3u4CAbd5Oc2EaZB5KjhOcEo6GjhGKcLS430y%2FXhqWo6dDWWYal1Ulu7CCvNLir0Q5aQ2m8EJuafK%2BqwxSBCGXiWeT4HXqabyU34i70bQbgBSjvffaHr27HVrD4hxq0DspKTIpf0fc4%2B%2Fs%2FTTbxEmX7FbVgmu3Y3rTpeNXegL2Lp5nrGp1drWpcM6g8PlrlhcWujdD2Sb0BpIO0dFNCmvOwDspKt8Ih7t0Sw3Q7u0EGEGJLt%2BgY7nrBoRm1G0DSBd0Ih4coHzfgoGBPy11atynvLwIDAQABo4IC%2FDCCAvgwHwYDVR0jBBgwFoAUUWj%2FkK8CB3U8zNllZGKiErhZcjswHQYDVR0OBBYEFNFZBVr1ptzC%2FiUkvSIQ8QAGfB5jMCUGA1UdEQQeMByCDSoudHdpdHRlci5jb22CC3R3aXR0ZXIuY29tMA4GA1UdDwEB%2FwQEAwIFoDAdBgNVHSUEFjAUBggrBgEFBQcDAQYIKwYBBQUHAwIwdQYDVR0fBG4wbDA0oDKgMIYuaHR0cDovL2NybDMuZGlnaWNlcnQuY29tL3NoYTItaGEtc2VydmVyLWc2LmNybDA0oDKgMIYuaHR0cDovL2NybDQuZGlnaWNlcnQuY29tL3NoYTItaGEtc2VydmVyLWc2LmNybDBMBgNVHSAERTBDMDcGCWCGSAGG%2FWwBATAqMCgGCCsGAQUFBwIBFhxodHRwczovL3d3dy5kaWdpY2VydC5jb20vQ1BTMAgGBmeBDAECAjCBgwYIKwYBBQUHAQEEdzB1MCQGCCsGAQUFBzABhhhodHRwOi8vb2NzcC5kaWdpY2VydC5jb20wTQYIKwYBBQUHMAKGQWh0dHA6Ly9jYWNlcnRzLmRpZ2ljZXJ0LmNvbS9EaWdpQ2VydFNIQTJIaWdoQXNzdXJhbmNlU2VydmVyQ0EuY3J0MAwGA1UdEwEB%2FwQCMAAwggEFBgorBgEEAdZ5AgQCBIH2BIHzAPEAdgDuS723dc5guuFCaR%2Br4Z5mow9%2BX7By2IMAxHuJeqj9ywAAAWrrCDXmAAAEAwBHMEUCIQCSwzRMoifj1DjYLJuMOH%2BZ6coX83bWIgWgWynLUED87wIgFKdipP9ycAE8S9apXJxKQyp0RoA%2Ftca5%2BWHQE5VlF68AdwCHdb%2FnWXz4jEOZX73zbv9WjUdWNv9KtWDBtOr%2FXqCDDwAAAWrrCDakAAAEAwBIMEYCIQCcmFOfp58BDMFZZS%2FRVl2uiY94RcTozBrcmtkpoWhgkgIhAN%2BX3ODViV4HnN2ZpZ383mFaEHIjKACRdWTRL%2BcYA43lMA0GCSqGSIb3DQEBCwUAA4IBAQB8LQLmUzevGtdw%2FXVL8aFrotloF3zsUPb2Q7CONLzlMGrmfv%2BNKDXN9xqPLLzmX4MddOjfpbVcDX1mJkuvuYNhh5AQrsaxcfEgfMcqPOdELA1mHPfk8H5a8Tvno1nF9wQWqkCQjLedjKReivmDOGfs1%2Fl%2FDnrprc0v4f28cWvN7YkJsdqfEGLbJ6SKsAxNbtNn%2Ffi6LxnemjLbcTrZYps11Ex55PtDNc200ejqpbpzHolgAew4KUzsyElaYSvDwkLMdmfwYSI9wgcPmrPBfZ0N12TAfmJJbE%2FkZc0jIOi92A%2FVgZLKvQrxadnBqPzAR%2F2SrZMT5joWa5sHpwl9fg5Z";

// both invalids
const errorCerts1 = "about:certificate?cert=aa&cert=bb&cert=abc";
// one invalid and other valid
const errorCerts2 =
  "about:certificate?cert=aa&cert=MIIGRjCCBS6gAwIBAgIQDJduPkI49CDWPd%2BG7%2Bu6kDANBgkqhkiG9w0BAQsFADBNMQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMScwJQYDVQQDEx5EaWdpQ2VydCBTSEEyIFNlY3VyZSBTZXJ2ZXIgQ0EwHhcNMTgxMTA1MDAwMDAwWhcNMTkxMTEzMTIwMDAwWjCBgzELMAkGA1UEBhMCVVMxEzARBgNVBAgTCkNhbGlmb3JuaWExFjAUBgNVBAcTDU1vdW50YWluIFZpZXcxHDAaBgNVBAoTE01vemlsbGEgQ29ycG9yYXRpb24xDzANBgNVBAsTBldlYk9wczEYMBYGA1UEAxMPd3d3Lm1vemlsbGEub3JnMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAuKruymkkmkqCJh7QjmXlUOBcLFRyw5LG%2FvUUWVrsxC2gsbR8WJq%2BcYoYBpoNVStKrO4U2rBh1GEbccvT6qKOQI%2BpjjDxx9cmRdubGTGp8L0MF1ohVvhIvYLumOEoRDDPU4PvGJjGhek%2FojvedPWe8dhciHkxOC2qPFZvVFMwg1%2Fo%2Fb80147BwZQmzB18mnHsmcyKlpsCN8pxw86uao9Iun8gZQrsllW64rTZlRR56pHdAcuGAoZjYZxwS9Z%2BlvrSjEgrddemWyGGalqyFp1rXlVM1Tf4%2FIYWAQXTgTUN303u3xMjss7QK7eUDsACRxiWPLW9XQDd1c%2ByvaYJKzgJ2wIDAQABo4IC6TCCAuUwHwYDVR0jBBgwFoAUD4BhHIIxYdUvKOeNRji0LOHG2eIwHQYDVR0OBBYEFNpSvSGcN2VT%2FB9TdQ8eXwebo60%2FMCcGA1UdEQQgMB6CD3d3dy5tb3ppbGxhLm9yZ4ILbW96aWxsYS5vcmcwDgYDVR0PAQH%2FBAQDAgWgMB0GA1UdJQQWMBQGCCsGAQUFBwMBBggrBgEFBQcDAjBrBgNVHR8EZDBiMC%2BgLaArhilodHRwOi8vY3JsMy5kaWdpY2VydC5jb20vc3NjYS1zaGEyLWc2LmNybDAvoC2gK4YpaHR0cDovL2NybDQuZGlnaWNlcnQuY29tL3NzY2Etc2hhMi1nNi5jcmwwTAYDVR0gBEUwQzA3BglghkgBhv1sAQEwKjAoBggrBgEFBQcCARYcaHR0cHM6Ly93d3cuZGlnaWNlcnQuY29tL0NQUzAIBgZngQwBAgIwfAYIKwYBBQUHAQEEcDBuMCQGCCsGAQUFBzABhhhodHRwOi8vb2NzcC5kaWdpY2VydC5jb20wRgYIKwYBBQUHMAKGOmh0dHA6Ly9jYWNlcnRzLmRpZ2ljZXJ0LmNvbS9EaWdpQ2VydFNIQTJTZWN1cmVTZXJ2ZXJDQS5jcnQwDAYDVR0TAQH%2FBAIwADCCAQIGCisGAQQB1nkCBAIEgfMEgfAA7gB1AKS5CZC0GFgUh7sTosxncAo8NZgE%2BRvfuON3zQ7IDdwQAAABZuYWiHwAAAQDAEYwRAIgZnMSH1JdG6NASHWTwD0mlP%2Fzbr0hzP263c02Ym0DU64CIEe4QHJDP47j0b6oTFu6RrZz1NQ9cq8Az1KnMKRuaFAlAHUAh3W%2F51l8%2BIxDmV%2B9827%2FVo1HVjb%2FSrVgwbTq%2F16ggw8AAAFm5haJAgAABAMARjBEAiAxGLXkUaOAkZhXNeNR3pWyahZeKmSaMXadgu18SfK1ZAIgKtwu5eGxK76rgaszLCZ9edBIjuU0DKorzPUuxUXFY0QwDQYJKoZIhvcNAQELBQADggEBAKLJAFO3wuaP5MM%2Fed1lhk5Uc2aDokhcM7XyvdhEKSHbgPhcgMoT9YIVoPa70gNC6KHcwoXu0g8wt7X6Vm1ql%2F68G5q844kFuC6JPl4LVT9mciD%2BVW6bHUSXD9xifL9DqdJ0Ic0SllTlM%2Boq5aAeOxUQGXhXIqj6fSQv9fQN6mXxQIoc%2Fgjxteskq%2FVl8YmY1FIZP9Bh7g27kxZ9GAAGQtjTL03RzKAuSg6yeImYVdQWasc7UPnBXlRAzZ8%2BOJThUbzK16a2CI3Rg4agKSJk%2BuA47h1%2FImmngpFLRb%2FMvRX6H1oWcUuyH6O7PZdl0YpwTpw1THIuqCGl%2FwpPgyQgcTM%3D";

async function checkSubjectName(inputPage, subjectsNameInfo) {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: inputPage,
    },
    async function(browser) {
      await ContentTask.spawn(browser, subjectsNameInfo, async function(
        subjectsNameInfoExpected
      ) {
        let certificateSection = await ContentTaskUtils.waitForCondition(() => {
          return content.document.querySelector("certificate-section");
        }, "Certificate section found");

        async function selectTab(tabName, index) {
          let tabs = certificateSection.shadowRoot.querySelector(
            ".certificate-tabs"
          );

          let tab = tabs.querySelector(`#tab${index}`);
          Assert.ok(tab, `Tab at index ${index} found`);
          Assert.equal(tab.innerText, tabName, `Tab name should be ${tabName}`);

          tab.click();
        }

        function checkSelectedTab(sectionItemsExpected, index) {
          let infoGroup = certificateSection.shadowRoot.querySelector(
            `#panel${index} info-group`
          );
          Assert.ok(infoGroup, "infoGroup found");

          let sectionTitle = infoGroup.shadowRoot.querySelector(
            ".info-group-title"
          ).innerText;
          Assert.equal(
            sectionTitle,
            "Subject Name",
            "Subject Name must be the selected item"
          );

          let infoItems = infoGroup.shadowRoot.querySelectorAll("info-item");
          Assert.equal(
            infoItems.length,
            sectionItemsExpected.length,
            "sectionItems must be the same length"
          );
        }

        for (let i = 0; i < subjectsNameInfoExpected.length; i++) {
          await selectTab(subjectsNameInfoExpected[i].tabName, i);
          checkSelectedTab(subjectsNameInfoExpected[i].sectionItems, i);
        }
      });
    }
  );
}

async function checkTabsName(inputPage, tabsNames) {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: inputPage,
    },
    async function(browser) {
      await ContentTask.spawn(browser, tabsNames, async function(
        expectedTabsNames
      ) {
        let certificateSection = await ContentTaskUtils.waitForCondition(() => {
          return content.document.querySelector("certificate-section");
        }, "Certificate section found");

        let tabsSection = certificateSection.shadowRoot.querySelector(
          ".certificate-tabs"
        );
        Assert.ok(tabsSection, "Tabs section found");

        let tabs = tabsSection.children;
        Assert.equal(
          tabs.length,
          expectedTabsNames.length,
          `There must be ${expectedTabsNames.length} tabs`
        );

        for (let i = 0; i < expectedTabsNames.length; i++) {
          Assert.equal(
            tabs[i].innerText,
            expectedTabsNames[i],
            "Tab name must be equal to expected tab name"
          );
          if (i === 0) {
            Assert.ok(
              tabs[i].className.includes("selected"),
              "First tab must be selected"
            );
          } else {
            Assert.equal(
              tabs[i].className.includes("selected"),
              false,
              "Just the first tab must be selected"
            );
          }
        }
      });
    }
  );
}

async function checkDOM(inputPage, errorExpected) {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: inputPage,
    },
    async function(browser) {
      await ContentTask.spawn(browser, errorExpected, async function(
        errorExpected
      ) {
        let certificateSection = await ContentTaskUtils.waitForCondition(() => {
          return content.document.querySelector("certificate-section");
        }, "Certificate section found");

        let errorSection = certificateSection.shadowRoot.querySelector(
          "error-section"
        );

        if (errorExpected) {
          // should render error page
          Assert.ok(errorSection, "Error section found");
        } else {
          // should render page with certificate info
          Assert.equal(errorSection, null, "Error section was not found");
        }
      });
    }
  );
}

add_task(async function runTests() {
  let inputs = [
    {
      url: multipleCerts1,
      tabsNames: ["www.mozilla.org", "github.com"],
      errorPageExpected: false,
      subjectNameInfo: [
        {
          tabName: "www.mozilla.org",
          sectionTitle: "Subject Name",
          sectionItems: [
            { label: "Country", info: "US" },
            { label: "State / Province", info: "California" },
            { label: "Locality", info: "Mountain View" },
            { label: "Organization", info: "Mozilla Corporation" },
            { label: "Organizational Unit", info: "WebOps" },
            { label: "Common Name", info: "www.mozilla.org" },
          ],
        },
        {
          tabName: "github.com",
          sectionTitle: "Subject Name",
          sectionItems: [
            { label: "Business Category", info: "Private Organization" },
            { label: "Inc. Country", info: "US" },
            { label: "Inc. State / Province", info: "Delaware" },
            { label: "Serial Number", info: "5157550" },
            { label: "Country", info: "US" },
            { label: "State / Province", info: "California" },
            { label: "Locality", info: "San Francisco" },
            { label: "Organization", info: "GitHub, Inc." },
            { label: "Common Name", info: "github.com" },
          ],
        },
      ],
    },
    {
      url: multipleCerts2,
      tabsNames: ["*.google.com", "*.twitter.com"],
      errorPageExpected: false,
      subjectNameInfo: [
        {
          tabName: "*.google.com",
          sectionTitle: "Subject Name",
          sectionItems: [
            { label: "Country", info: "US" },
            { label: "State / Province", info: "California" },
            { label: "Locality", info: "Mountain View" },
            { label: "Organization", info: "Google LLC" },
            { label: "Common Name", info: "*.google.com" },
          ],
        },
        {
          tabName: "*.twitter.com",
          sectionTitle: "Subject Name",
          sectionItems: [
            { label: "Country", info: "US" },
            { label: "State / Province", info: "California" },
            { label: "Locality", info: "San Francisco" },
            { label: "Organization", info: "Twitter, Inc." },
            { label: "Organizational Unit", info: "atla" },
            { label: "Common Name", info: "*.twitter.com" },
          ],
        },
      ],
    },
    {
      url: errorCerts1,
      tabsNames: [],
      errorPageExpected: true,
    },
    {
      url: errorCerts2,
      tabsNames: [],
      errorPageExpected: true,
    },
  ];

  for (let input of inputs) {
    await checkDOM(input.url, input.errorPageExpected);
    if (!input.errorPageExpected) {
      await checkTabsName(input.url, input.tabsNames);
      await checkSubjectName(input.url, input.subjectNameInfo);
    }
  }
});

if(TARGET python-fd)
  check_pymodule_available(2 psycopg2)
  check_pymodule_available(2 dateutil)
  if(PYMODULE_2_PSYCOPG2_FOUND AND PYMODULE_2_DATEUTIL_FOUND)
    create_systemtest(${SYSTEMTEST_PREFIX} "py2plug-fd-postgres")
  else()
    create_systemtest(${SYSTEMTEST_PREFIX} "py2plug-fd-postgres" DISABLED)
  endif()
endif()

if(TARGET python3-fd)
  check_pymodule_available(3 psycopg2)
  check_pymodule_available(3 dateutil)
  if(PYMODULE_3_PSYCOPG2_FOUND
     AND PYMODULE_3_DATEUTIL_FOUND
     AND (${Python3_VERSION_MAJOR} EQUAL 3)
     AND (${Python3_VERSION_MINOR} LESS 10)
  )
    create_systemtest(${SYSTEMTEST_PREFIX} "py3plug-fd-postgres")
    # fails because of psycopg2
    set_tests_properties(
      "${SYSTEMTEST_PREFIX}py3plug-fd-postgres" PROPERTIES LABELS "broken"
    )
  else()
    create_systemtest(${SYSTEMTEST_PREFIX} "py3plug-fd-postgres" DISABLED)
  endif()
endif()

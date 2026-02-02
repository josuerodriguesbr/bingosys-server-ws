--
-- PostgreSQL database dump
--

\restrict m9aunUHiL0tD8D9NGlxqqij3oRvHOCQVpZ152LIU0b6ABGJHucDnbTWxSycDbXp

-- Dumped from database version 16.11 (Ubuntu 16.11-0ubuntu0.24.04.1)
-- Dumped by pg_dump version 16.11 (Ubuntu 16.11-0ubuntu0.24.04.1)

SET statement_timeout = 0;
SET lock_timeout = 0;
SET idle_in_transaction_session_timeout = 0;
SET client_encoding = 'UTF8';
SET standard_conforming_strings = on;
SELECT pg_catalog.set_config('search_path', '', false);
SET check_function_bodies = false;
SET xmloption = content;
SET client_min_messages = warning;
SET row_security = off;

SET default_tablespace = '';

SET default_table_access_method = heap;

--
-- Name: bases_dados; Type: TABLE; Schema: public; Owner: postgres
--

CREATE TABLE public.bases_dados (
    id integer NOT NULL,
    nome character varying(255) NOT NULL,
    tipo_grade character varying(50) NOT NULL,
    caminho_dados character varying(255)
);


ALTER TABLE public.bases_dados OWNER TO postgres;

--
-- Name: bases_dados_id_seq; Type: SEQUENCE; Schema: public; Owner: postgres
--

CREATE SEQUENCE public.bases_dados_id_seq
    AS integer
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


ALTER SEQUENCE public.bases_dados_id_seq OWNER TO postgres;

--
-- Name: bases_dados_id_seq; Type: SEQUENCE OWNED BY; Schema: public; Owner: postgres
--

ALTER SEQUENCE public.bases_dados_id_seq OWNED BY public.bases_dados.id;


--
-- Name: bolas_sorteadas; Type: TABLE; Schema: public; Owner: postgres
--

CREATE TABLE public.bolas_sorteadas (
    id integer NOT NULL,
    sorteio_id integer,
    numero integer NOT NULL,
    momento timestamp with time zone DEFAULT CURRENT_TIMESTAMP
);


ALTER TABLE public.bolas_sorteadas OWNER TO postgres;

--
-- Name: bolas_sorteadas_id_seq; Type: SEQUENCE; Schema: public; Owner: postgres
--

CREATE SEQUENCE public.bolas_sorteadas_id_seq
    AS integer
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


ALTER SEQUENCE public.bolas_sorteadas_id_seq OWNER TO postgres;

--
-- Name: bolas_sorteadas_id_seq; Type: SEQUENCE OWNED BY; Schema: public; Owner: postgres
--

ALTER SEQUENCE public.bolas_sorteadas_id_seq OWNED BY public.bolas_sorteadas.id;


--
-- Name: cartelas_validadas; Type: TABLE; Schema: public; Owner: postgres
--

CREATE TABLE public.cartelas_validadas (
    id integer NOT NULL,
    sorteio_id integer,
    numero_cartela integer NOT NULL,
    telefone_participante character varying(20),
    origem character varying(50)
);


ALTER TABLE public.cartelas_validadas OWNER TO postgres;

--
-- Name: cartelas_validadas_id_seq; Type: SEQUENCE; Schema: public; Owner: postgres
--

CREATE SEQUENCE public.cartelas_validadas_id_seq
    AS integer
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


ALTER SEQUENCE public.cartelas_validadas_id_seq OWNER TO postgres;

--
-- Name: cartelas_validadas_id_seq; Type: SEQUENCE OWNED BY; Schema: public; Owner: postgres
--

ALTER SEQUENCE public.cartelas_validadas_id_seq OWNED BY public.cartelas_validadas.id;


--
-- Name: chaves_acesso; Type: TABLE; Schema: public; Owner: postgres
--

CREATE TABLE public.chaves_acesso (
    id integer NOT NULL,
    codigo_chave character varying(100) NOT NULL,
    status character varying(50) DEFAULT 'ativa'::character varying,
    sorteio_id integer,
    criado_em timestamp with time zone DEFAULT CURRENT_TIMESTAMP,
    usado_em timestamp with time zone
);


ALTER TABLE public.chaves_acesso OWNER TO postgres;

--
-- Name: chaves_acesso_id_seq; Type: SEQUENCE; Schema: public; Owner: postgres
--

CREATE SEQUENCE public.chaves_acesso_id_seq
    AS integer
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


ALTER SEQUENCE public.chaves_acesso_id_seq OWNER TO postgres;

--
-- Name: chaves_acesso_id_seq; Type: SEQUENCE OWNED BY; Schema: public; Owner: postgres
--

ALTER SEQUENCE public.chaves_acesso_id_seq OWNED BY public.chaves_acesso.id;


--
-- Name: modelos_sorteio; Type: TABLE; Schema: public; Owner: postgres
--

CREATE TABLE public.modelos_sorteio (
    id integer NOT NULL,
    nome character varying(255) NOT NULL,
    config_padrao jsonb
);


ALTER TABLE public.modelos_sorteio OWNER TO postgres;

--
-- Name: modelos_sorteio_id_seq; Type: SEQUENCE; Schema: public; Owner: postgres
--

CREATE SEQUENCE public.modelos_sorteio_id_seq
    AS integer
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


ALTER SEQUENCE public.modelos_sorteio_id_seq OWNER TO postgres;

--
-- Name: modelos_sorteio_id_seq; Type: SEQUENCE OWNED BY; Schema: public; Owner: postgres
--

ALTER SEQUENCE public.modelos_sorteio_id_seq OWNED BY public.modelos_sorteio.id;


--
-- Name: premiacoes; Type: TABLE; Schema: public; Owner: postgres
--

CREATE TABLE public.premiacoes (
    id integer NOT NULL,
    sorteio_id integer,
    nome_premio character varying(255) NOT NULL,
    tipo character varying(50),
    padrao_grade jsonb,
    ordem_exibicao integer
);


ALTER TABLE public.premiacoes OWNER TO postgres;

--
-- Name: premiacoes_id_seq; Type: SEQUENCE; Schema: public; Owner: postgres
--

CREATE SEQUENCE public.premiacoes_id_seq
    AS integer
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


ALTER SEQUENCE public.premiacoes_id_seq OWNER TO postgres;

--
-- Name: premiacoes_id_seq; Type: SEQUENCE OWNED BY; Schema: public; Owner: postgres
--

ALTER SEQUENCE public.premiacoes_id_seq OWNED BY public.premiacoes.id;


--
-- Name: sorteios; Type: TABLE; Schema: public; Owner: postgres
--

CREATE TABLE public.sorteios (
    id integer NOT NULL,
    status character varying(50) DEFAULT 'configurando'::character varying,
    modelo_id integer,
    base_id integer,
    preferencias jsonb,
    criado_em timestamp with time zone DEFAULT CURRENT_TIMESTAMP,
    finalizado_em timestamp with time zone
);


ALTER TABLE public.sorteios OWNER TO postgres;

--
-- Name: sorteios_id_seq; Type: SEQUENCE; Schema: public; Owner: postgres
--

CREATE SEQUENCE public.sorteios_id_seq
    AS integer
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


ALTER SEQUENCE public.sorteios_id_seq OWNER TO postgres;

--
-- Name: sorteios_id_seq; Type: SEQUENCE OWNED BY; Schema: public; Owner: postgres
--

ALTER SEQUENCE public.sorteios_id_seq OWNED BY public.sorteios.id;


--
-- Name: bases_dados id; Type: DEFAULT; Schema: public; Owner: postgres
--

ALTER TABLE ONLY public.bases_dados ALTER COLUMN id SET DEFAULT nextval('public.bases_dados_id_seq'::regclass);


--
-- Name: bolas_sorteadas id; Type: DEFAULT; Schema: public; Owner: postgres
--

ALTER TABLE ONLY public.bolas_sorteadas ALTER COLUMN id SET DEFAULT nextval('public.bolas_sorteadas_id_seq'::regclass);


--
-- Name: cartelas_validadas id; Type: DEFAULT; Schema: public; Owner: postgres
--

ALTER TABLE ONLY public.cartelas_validadas ALTER COLUMN id SET DEFAULT nextval('public.cartelas_validadas_id_seq'::regclass);


--
-- Name: chaves_acesso id; Type: DEFAULT; Schema: public; Owner: postgres
--

ALTER TABLE ONLY public.chaves_acesso ALTER COLUMN id SET DEFAULT nextval('public.chaves_acesso_id_seq'::regclass);


--
-- Name: modelos_sorteio id; Type: DEFAULT; Schema: public; Owner: postgres
--

ALTER TABLE ONLY public.modelos_sorteio ALTER COLUMN id SET DEFAULT nextval('public.modelos_sorteio_id_seq'::regclass);


--
-- Name: premiacoes id; Type: DEFAULT; Schema: public; Owner: postgres
--

ALTER TABLE ONLY public.premiacoes ALTER COLUMN id SET DEFAULT nextval('public.premiacoes_id_seq'::regclass);


--
-- Name: sorteios id; Type: DEFAULT; Schema: public; Owner: postgres
--

ALTER TABLE ONLY public.sorteios ALTER COLUMN id SET DEFAULT nextval('public.sorteios_id_seq'::regclass);


--
-- Name: bases_dados bases_dados_pkey; Type: CONSTRAINT; Schema: public; Owner: postgres
--

ALTER TABLE ONLY public.bases_dados
    ADD CONSTRAINT bases_dados_pkey PRIMARY KEY (id);


--
-- Name: bolas_sorteadas bolas_sorteadas_pkey; Type: CONSTRAINT; Schema: public; Owner: postgres
--

ALTER TABLE ONLY public.bolas_sorteadas
    ADD CONSTRAINT bolas_sorteadas_pkey PRIMARY KEY (id);


--
-- Name: cartelas_validadas cartelas_validadas_pkey; Type: CONSTRAINT; Schema: public; Owner: postgres
--

ALTER TABLE ONLY public.cartelas_validadas
    ADD CONSTRAINT cartelas_validadas_pkey PRIMARY KEY (id);


--
-- Name: chaves_acesso chaves_acesso_codigo_chave_key; Type: CONSTRAINT; Schema: public; Owner: postgres
--

ALTER TABLE ONLY public.chaves_acesso
    ADD CONSTRAINT chaves_acesso_codigo_chave_key UNIQUE (codigo_chave);


--
-- Name: chaves_acesso chaves_acesso_pkey; Type: CONSTRAINT; Schema: public; Owner: postgres
--

ALTER TABLE ONLY public.chaves_acesso
    ADD CONSTRAINT chaves_acesso_pkey PRIMARY KEY (id);


--
-- Name: modelos_sorteio modelos_sorteio_pkey; Type: CONSTRAINT; Schema: public; Owner: postgres
--

ALTER TABLE ONLY public.modelos_sorteio
    ADD CONSTRAINT modelos_sorteio_pkey PRIMARY KEY (id);


--
-- Name: premiacoes premiacoes_pkey; Type: CONSTRAINT; Schema: public; Owner: postgres
--

ALTER TABLE ONLY public.premiacoes
    ADD CONSTRAINT premiacoes_pkey PRIMARY KEY (id);


--
-- Name: sorteios sorteios_pkey; Type: CONSTRAINT; Schema: public; Owner: postgres
--

ALTER TABLE ONLY public.sorteios
    ADD CONSTRAINT sorteios_pkey PRIMARY KEY (id);


--
-- Name: bolas_sorteadas bolas_sorteadas_sorteio_id_fkey; Type: FK CONSTRAINT; Schema: public; Owner: postgres
--

ALTER TABLE ONLY public.bolas_sorteadas
    ADD CONSTRAINT bolas_sorteadas_sorteio_id_fkey FOREIGN KEY (sorteio_id) REFERENCES public.sorteios(id);


--
-- Name: cartelas_validadas cartelas_validadas_sorteio_id_fkey; Type: FK CONSTRAINT; Schema: public; Owner: postgres
--

ALTER TABLE ONLY public.cartelas_validadas
    ADD CONSTRAINT cartelas_validadas_sorteio_id_fkey FOREIGN KEY (sorteio_id) REFERENCES public.sorteios(id);


--
-- Name: chaves_acesso chaves_acesso_sorteio_id_fkey; Type: FK CONSTRAINT; Schema: public; Owner: postgres
--

ALTER TABLE ONLY public.chaves_acesso
    ADD CONSTRAINT chaves_acesso_sorteio_id_fkey FOREIGN KEY (sorteio_id) REFERENCES public.sorteios(id);


--
-- Name: premiacoes premiacoes_sorteio_id_fkey; Type: FK CONSTRAINT; Schema: public; Owner: postgres
--

ALTER TABLE ONLY public.premiacoes
    ADD CONSTRAINT premiacoes_sorteio_id_fkey FOREIGN KEY (sorteio_id) REFERENCES public.sorteios(id);


--
-- Name: sorteios sorteios_base_id_fkey; Type: FK CONSTRAINT; Schema: public; Owner: postgres
--

ALTER TABLE ONLY public.sorteios
    ADD CONSTRAINT sorteios_base_id_fkey FOREIGN KEY (base_id) REFERENCES public.bases_dados(id);


--
-- Name: sorteios sorteios_modelo_id_fkey; Type: FK CONSTRAINT; Schema: public; Owner: postgres
--

ALTER TABLE ONLY public.sorteios
    ADD CONSTRAINT sorteios_modelo_id_fkey FOREIGN KEY (modelo_id) REFERENCES public.modelos_sorteio(id);


--
-- PostgreSQL database dump complete
--

\unrestrict m9aunUHiL0tD8D9NGlxqqij3oRvHOCQVpZ152LIU0b6ABGJHucDnbTWxSycDbXp

